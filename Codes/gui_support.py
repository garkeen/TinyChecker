from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from .elaborator import ElabLocalContext, Elaborator, extend_local
from .errors import ElabError, ParseError, TinyCheckerError, TypeCheckError
from .lexer import Token, tokenize
from .parser import NameSite, ParseArtifacts, parse_with_metadata
from .restricted import RDataCtorDecl, RDefinition, REliminatorDecl, RTypeCtorDecl, fold_lam_expr, split_pi_expr
from .surface import (
    AppExpr,
    ArrowExpr,
    AtomExpr,
    AxiomDecl,
    CaseBranch,
    CaseExpr,
    CtorDecl,
    Decl,
    EqExpr,
    EquationDecl,
    ExampleDecl,
    Expr,
    FieldDecl,
    FunDecl,
    InductiveDecl,
    LambdaExpr,
    LetExpr,
    MatchBranch,
    MatchExpr,
    Param,
    PiExpr,
    ProductDecl,
    ProductExpr,
    VarDecl,
    unfold_surface_app,
)


@dataclass(frozen=True)
class TokenFocus:
    index: int


@dataclass(frozen=True)
class GapFocus:
    index: int


Focus = TokenFocus | GapFocus


@dataclass(frozen=True)
class GuiLocalEntry:
    name: str
    typ: str


@dataclass(frozen=True)
class GuiGlobalEntry:
    name: str
    typ: str
    tag: str


@dataclass(frozen=True)
class GuiBranchFrame:
    kind: str
    constructor: str
    fields: list[str]
    ihs: list[str]


@dataclass(frozen=True)
class GuiProbeResult:
    target_expr: Optional[Expr]
    target_name_site: Optional[NameSite]
    target_text: str
    target_kind: str
    local_context: list[GuiLocalEntry]
    global_context: list[GuiGlobalEntry]
    resolved_name: Optional[str]
    lowered_expr: Optional[str]
    branch_stack: list[GuiBranchFrame]


@dataclass(frozen=True)
class GuiAnalysis:
    source: str
    tokens: list[Token]
    artifacts: ParseArtifacts
    focus: Focus
    decl: Optional[Decl]
    target_expr: Optional[Expr]
    target_name_site: Optional[NameSite]
    target_span: Optional[tuple[int, int]]
    target_offsets: Optional[tuple[int, int]]
    probe: Optional[GuiProbeResult]
    minimal_expr: Optional[Expr]
    target_parent_id: Optional[int]
    compile_status: Optional[str]
    proof_nodes: Optional[list] = None
    error: Optional[str] = None


def visible_tokens(tokens: list[Token]) -> list[Token]:
    return [token for token in tokens if token.kind != "EOF"]


def cursor_focus(tokens: list[Token], cursor_pos: int) -> Focus:
    visible = visible_tokens(tokens)
    if not visible:
        return GapFocus(0)
    for index, token in enumerate(visible):
        if token.start_pos <= cursor_pos < token.end_pos:
            return TokenFocus(index)
    if cursor_pos <= visible[0].start_pos:
        return GapFocus(0)
    for index in range(1, len(visible)):
        left = visible[index - 1]
        right = visible[index]
        if left.end_pos <= cursor_pos <= right.start_pos:
            return GapFocus(index)
    return GapFocus(len(visible))


def span_contains_focus(span: tuple[int, int], focus: Focus) -> bool:
    start, end = span
    if isinstance(focus, TokenFocus):
        return start <= focus.index < end
    return start < focus.index < end


def decl_for_focus(decls: list[Decl], spans: dict[int, tuple[int, int]], focus: Focus) -> Optional[Decl]:
    candidates = [decl for decl in decls if span_contains_focus(spans.get(decl.surface_id, (-1, -1)), focus)]
    if candidates:
        return min(candidates, key=lambda decl: spans[decl.surface_id][1] - spans[decl.surface_id][0])
    if isinstance(focus, GapFocus):
        if focus.index > 0:
            fallback = decl_for_focus(decls, spans, TokenFocus(focus.index - 1))
            if fallback is not None:
                return fallback
        return decl_for_focus(decls, spans, TokenFocus(focus.index))
    return None


def expr_children(expr: Expr) -> list[Expr]:
    if isinstance(expr, AtomExpr):
        return []
    if isinstance(expr, AppExpr):
        return [expr.func, expr.arg]
    if isinstance(expr, ArrowExpr):
        return [expr.domain, expr.codomain]
    if isinstance(expr, PiExpr):
        return [expr.domain, expr.codomain]
    if isinstance(expr, LambdaExpr):
        return [expr.param.typ, expr.body]
    if isinstance(expr, LetExpr):
        return [expr.typ, expr.value, expr.body]
    if isinstance(expr, EqExpr):
        return [expr.typ, expr.lhs, expr.rhs]
    if isinstance(expr, ProductExpr):
        return list(expr.args)
    if isinstance(expr, MatchExpr):
        items = [expr.scrutinee, *expr.family_args, expr.motive_body]
        items.extend(branch.body for branch in expr.branches)
        return items
    if isinstance(expr, CaseExpr):
        items = [expr.scrutinee, *expr.type_args, expr.motive_body]
        items.extend(branch.body for branch in expr.branches)
        return items
    return []


def exprs_in_decl(decl: Decl) -> list[Expr]:
    result: list[Expr] = []

    def visit(expr: Expr) -> None:
        result.append(expr)
        for child in expr_children(expr):
            visit(child)

    if isinstance(decl, VarDecl):
        visit(decl.typ)
        visit(decl.value)
    elif isinstance(decl, FunDecl):
        for param in decl.params:
            visit(param.typ)
        visit(decl.ret_type)
        visit(decl.body)
    elif isinstance(decl, InductiveDecl):
        for param in decl.params:
            visit(param.typ)
        if decl.arity is not None:
            visit(decl.arity)
        for ctor in decl.ctors:
            visit(ctor.typ)
    elif isinstance(decl, ProductDecl):
        for field in decl.fields:
            visit(field.typ)
    elif isinstance(decl, AxiomDecl):
        visit(decl.typ)
    elif isinstance(decl, ExampleDecl):
        visit(decl.typ)
        visit(decl.value)
    elif isinstance(decl, EquationDecl):
        visit(decl.typ)
        visit(decl.value)
    return result


def expr_parent_map(decl: Decl) -> dict[int, int]:
    parents: dict[int, int] = {}

    def visit(expr: Expr) -> None:
        for child in expr_children(expr):
            parents[child.surface_id] = expr.surface_id
            visit(child)

    for expr in exprs_in_decl(decl):
        # Only visit roots once; child recursion will cover the rest.
        pass

    roots: list[Expr] = []
    if isinstance(decl, VarDecl):
        roots = [decl.typ, decl.value]
    elif isinstance(decl, FunDecl):
        roots = [*(param.typ for param in decl.params), decl.ret_type, decl.body]
    elif isinstance(decl, InductiveDecl):
        roots = [*(param.typ for param in decl.params)]
        if decl.arity is not None:
            roots.append(decl.arity)
        roots.extend(ctor.typ for ctor in decl.ctors)
    elif isinstance(decl, ProductDecl):
        roots = [field.typ for field in decl.fields]
    elif isinstance(decl, AxiomDecl):
        roots = [decl.typ]
    elif isinstance(decl, ExampleDecl):
        roots = [decl.typ, decl.value]
    elif isinstance(decl, EquationDecl):
        roots = [decl.typ, decl.value]
    for root in roots:
        visit(root)
    return parents


def find_expr_by_id(decl: Decl, surface_id: int) -> Optional[Expr]:
    for expr in exprs_in_decl(decl):
        if expr.surface_id == surface_id:
            return expr
    return None


def select_expr_for_focus(decl: Decl, spans: dict[int, tuple[int, int]], focus: Focus) -> Optional[Expr]:
    exprs = exprs_in_decl(decl)
    candidates = [expr for expr in exprs if span_contains_focus(spans.get(expr.surface_id, (-1, -1)), focus)]
    if candidates:
        return min(candidates, key=lambda expr: spans[expr.surface_id][1] - spans[expr.surface_id][0])
    if isinstance(focus, GapFocus):
        if focus.index > 0:
            fallback = select_expr_for_focus(decl, spans, TokenFocus(focus.index - 1))
            if fallback is not None:
                return fallback
        return select_expr_for_focus(decl, spans, TokenFocus(focus.index))
    return None


def select_name_site_for_focus(name_sites: list[NameSite], focus: Focus) -> Optional[NameSite]:
    if isinstance(focus, GapFocus):
        return None
    candidates = [
        site
        for site in name_sites
        if site.token_span[0] <= focus.index < site.token_span[1]
    ]
    if not candidates:
        return None
    return min(candidates, key=lambda site: site.token_span[1] - site.token_span[0])


def token_span_to_offsets(tokens: list[Token], span: tuple[int, int]) -> Optional[tuple[int, int]]:
    visible = visible_tokens(tokens)
    start, end = span
    if start < 0 or end <= start or end > len(visible):
        return None
    return visible[start].start_pos, visible[end - 1].end_pos


def show_expr(expr: Expr) -> str:
    prec_binder = 10
    prec_arrow = 20
    prec_eq = 30
    prec_app = 40
    prec_atom = 50

    def par(text: str, need: bool) -> str:
        return f"({text})" if need else text

    def expr_prec(node: Expr) -> int:
        if isinstance(node, AtomExpr | ProductExpr):
            return prec_atom
        if isinstance(node, AppExpr):
            return prec_app
        if isinstance(node, EqExpr):
            return prec_eq
        if isinstance(node, ArrowExpr | PiExpr):
            return prec_arrow
        if isinstance(node, LambdaExpr | LetExpr | MatchExpr | CaseExpr):
            return prec_binder
        return 0

    def render(node: Expr, ctx_prec: int) -> str:
        if isinstance(node, AtomExpr):
            return node.text
        if isinstance(node, ProductExpr):
            args = ", ".join(render(arg, 0) for arg in node.args)
            return f"{node.type_name}<{args}>"
        if isinstance(node, AppExpr):
            head, args = unfold_surface_app(node)
            head_text = render(head, prec_app)
            arg_texts = [render(arg, prec_app + 1) for arg in args]
            text = " ".join([head_text, *arg_texts])
            return par(text, prec_app < ctx_prec)
        if isinstance(node, ArrowExpr):
            domain = render(node.domain, prec_arrow + 1)
            codomain = render(node.codomain, prec_arrow)
            text = f"{domain} -> {codomain}"
            return par(text, prec_arrow < ctx_prec)
        if isinstance(node, PiExpr):
            name = node.name or "_"
            domain = render(node.domain, 0)
            codomain = render(node.codomain, prec_arrow)
            text = f"({name}:{domain}) -> {codomain}"
            return par(text, prec_arrow < ctx_prec)
        if isinstance(node, LambdaExpr):
            name = node.param.name or "_"
            param_type = render(node.param.typ, 0)
            body = render(node.body, prec_binder)
            text = f"λ{name}:{param_type}.{body}"
            return par(text, prec_binder < ctx_prec)
        if isinstance(node, LetExpr):
            name = node.name or "_"
            text = (
                f"let {name}:{render(node.typ, 0)} = {render(node.value, 0)} "
                f"in {render(node.body, prec_binder)}"
            )
            return par(text, prec_binder < ctx_prec)
        if isinstance(node, EqExpr):
            text = (
                f"[{render(node.typ, 0)}] {render(node.lhs, prec_eq + 1)}"
                f" == {render(node.rhs, prec_eq + 1)}"
            )
            return par(text, prec_eq < ctx_prec)
        if isinstance(node, MatchExpr):
            text = f"match {render(node.scrutinee, 0)} ... end"
            return par(text, prec_binder < ctx_prec)
        if isinstance(node, CaseExpr):
            text = f"case {render(node.scrutinee, 0)} ... end"
            return par(text, prec_binder < ctx_prec)
        return repr(node)

    return render(expr, 0)


class _ProbeFound(Exception):
    pass


class GuiProbeElaborator(Elaborator):
    def __init__(self, target_expr_id: Optional[int], target_name_site: Optional[NameSite]) -> None:
        super().__init__()
        self.target_expr_id = target_expr_id
        self.target_name_site = target_name_site
        self.result: Optional[GuiProbeResult] = None
        self.global_types: dict[str, str] = {}

    def record_decl_types(self, decls) -> None:
        for decl in decls:
            if isinstance(decl, RDefinition):
                self.global_types[decl.name] = show_expr(decl.typ)
            elif isinstance(decl, RTypeCtorDecl):
                self.global_types[decl.name] = show_expr(decl.typ)
            elif isinstance(decl, RDataCtorDecl):
                self.global_types[decl.name] = show_expr(decl.typ)
            elif isinstance(decl, REliminatorDecl):
                self.global_types[decl.name] = show_expr(decl.typ)

    def snapshot_global_context(self) -> list[GuiGlobalEntry]:
        names = sorted(self.state.global_tags)
        result: list[GuiGlobalEntry] = []
        for name in names:
            result.append(
                GuiGlobalEntry(
                    name=name,
                    typ=self.global_types.get(name, "<type unavailable>"),
                    tag=self.state.global_tags[name],
                )
            )
        return result

    def probe_program(self, decls: list[Decl], target_decl: Decl) -> Optional[GuiProbeResult]:
        for decl in decls:
            if decl is target_decl:
                try:
                    self.probe_decl(decl)
                except _ProbeFound:
                    return self.result
                return self.result
            self.record_decl_types(self.elab_decl(decl))
        return None

    def capture(
        self,
        expr: Expr,
        local_entries: list[tuple[str, Expr]],
        local_names: list[str],
        *,
        extra: Optional[set[str]] = None,
        branch_stack: Optional[list[GuiBranchFrame]] = None,
    ) -> None:
        lowered_expr: Optional[str]
        try:
            lowered = super().lower_expr(expr, ElabLocalContext(local_names), extra=extra)
            lowered_expr = show_expr(lowered)
        except TinyCheckerError:
            lowered_expr = None
        resolved_name: Optional[str] = None
        if isinstance(expr, AtomExpr):
            resolved_name = self.resolve_atom(expr.text, local_names, extra=extra)
        self.result = GuiProbeResult(
            target_expr=expr,
            target_name_site=None,
            target_text=show_expr(expr),
            target_kind=type(expr).__name__,
            local_context=[GuiLocalEntry(name, show_expr(typ)) for name, typ in reversed(local_entries)],
            global_context=self.snapshot_global_context(),
            resolved_name=resolved_name,
            lowered_expr=lowered_expr,
            branch_stack=[] if branch_stack is None else list(branch_stack),
        )
        raise _ProbeFound()

    def capture_name_site(
        self,
        site: NameSite,
        local_entries: list[tuple[str, Expr]],
        *,
        resolved_name: Optional[str],
        lowered_expr: Optional[str],
        branch_stack: Optional[list[GuiBranchFrame]] = None,
    ) -> None:
        self.result = GuiProbeResult(
            target_expr=None,
            target_name_site=site,
            target_text=site.name,
            target_kind=site.role,
            local_context=[GuiLocalEntry(name, show_expr(typ)) for name, typ in reversed(local_entries)],
            global_context=self.snapshot_global_context(),
            resolved_name=resolved_name,
            lowered_expr=lowered_expr,
            branch_stack=[] if branch_stack is None else list(branch_stack),
        )
        raise _ProbeFound()

    def resolve_atom(self, text: str, local_names: list[str], *, extra: Optional[set[str]]) -> Optional[str]:
        if text in {"Type", "Prop"}:
            return "Type"
        if text in local_names:
            return f"local:{text}"
        resolved = self.resolve_dot(text) if "." in text else None
        if resolved is not None:
            return f"global:{resolved}"
        if self.is_defined(text, extra=extra):
            return f"global:{text}" if text in self.state.global_tags else f"reserved:{text}"
        return None

    def probe_decl(self, decl: Decl) -> None:
        if isinstance(decl, VarDecl):
            self.probe_expr(decl.typ, [], [])
            self.probe_expr(decl.value, [], [])
            return
        if isinstance(decl, FunDecl):
            local_entries: list[tuple[str, Expr]] = []
            local_names: list[str] = []
            for param in decl.params:
                self.probe_expr(param.typ, local_entries, local_names)
                actual = self.actual_binder_name(param.name, "p")
                lowered_type = super().lower_expr(param.typ, ElabLocalContext(local_names))
                local_entries.append((actual, lowered_type))
                local_names.append(actual)
            self.probe_expr(decl.ret_type, local_entries, local_names)
            self.probe_expr(decl.body, local_entries, local_names)
            return
        if isinstance(decl, InductiveDecl):
            local_entries = []
            local_names = []
            for param in decl.params:
                self.probe_expr(param.typ, local_entries, local_names)
                actual = self.actual_binder_name(param.name, "p")
                lowered_type = super().lower_expr(param.typ, ElabLocalContext(local_names))
                local_entries.append((actual, lowered_type))
                local_names.append(actual)
            extra = {decl.name}
            if decl.arity is not None:
                self.probe_expr(decl.arity, local_entries, local_names, extra=extra)
            for ctor in decl.ctors:
                self.probe_expr(ctor.typ, local_entries, local_names, extra=extra)
            return
        if isinstance(decl, ProductDecl):
            local_entries = []
            local_names = []
            for field in decl.fields:
                self.probe_expr(field.typ, local_entries, local_names)
                lowered_type = super().lower_expr(field.typ, ElabLocalContext(local_names))
                local_entries.append((field.name, lowered_type))
                local_names.append(field.name)
            return
        if isinstance(decl, AxiomDecl):
            self.probe_expr(decl.typ, [], [])
            return
        if isinstance(decl, ExampleDecl):
            self.probe_expr(decl.typ, [], [])
            self.probe_expr(decl.value, [], [])
            return
        if isinstance(decl, EquationDecl):
            typ = super().lower_expr(decl.typ, ElabLocalContext())
            self.probe_expr(decl.typ, [], [])
            telescope, _ = split_pi_expr(typ)
            local_entries = []
            local_names = []
            for (_, binder_ty), raw_name in zip(telescope, decl.params):
                actual = self.actual_binder_name(raw_name, "eq")
                local_entries.append((actual, binder_ty))
                local_names.append(actual)
            self.probe_expr(decl.value, local_entries, local_names)
            return

    def probe_expr(
        self,
        expr: Expr,
        local_entries: list[tuple[str, Expr]],
        local_names: list[str],
        *,
        extra: Optional[set[str]] = None,
        branch_stack: Optional[list[GuiBranchFrame]] = None,
    ) -> None:
        if expr.surface_id == self.target_expr_id:
            self.capture(expr, local_entries, local_names, extra=extra, branch_stack=branch_stack)
        if isinstance(expr, AtomExpr):
            return
        if isinstance(expr, AppExpr):
            self.probe_expr(expr.func, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            self.probe_expr(expr.arg, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            return
        if isinstance(expr, ArrowExpr):
            self.probe_expr(expr.domain, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            self.probe_expr(expr.codomain, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            return
        if isinstance(expr, PiExpr):
            self.probe_expr(expr.domain, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            actual = self.actual_binder_name(expr.name, "pi")
            lowered_domain = super().lower_expr(expr.domain, ElabLocalContext(local_names), extra=extra)
            self.probe_expr(
                expr.codomain,
                local_entries + [(actual, lowered_domain)],
                local_names + [actual],
                extra=extra,
                branch_stack=branch_stack,
            )
            return
        if isinstance(expr, LambdaExpr):
            self.probe_expr(expr.param.typ, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            actual = self.actual_binder_name(expr.param.name, "lam")
            lowered_type = super().lower_expr(expr.param.typ, ElabLocalContext(local_names), extra=extra)
            self.probe_expr(
                expr.body,
                local_entries + [(actual, lowered_type)],
                local_names + [actual],
                extra=extra,
                branch_stack=branch_stack,
            )
            return
        if isinstance(expr, LetExpr):
            self.probe_expr(expr.typ, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            self.probe_expr(expr.value, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            actual = self.actual_binder_name(expr.name, "let")
            lowered_type = super().lower_expr(expr.typ, ElabLocalContext(local_names), extra=extra)
            if (
                self.target_name_site is not None
                and self.target_name_site.role == "let_name"
                and self.target_name_site.owner_surface_id == expr.surface_id
            ):
                self.capture_name_site(
                    self.target_name_site,
                    local_entries + [(actual, lowered_type)],
                    resolved_name=f"binder:{actual}",
                    lowered_expr=show_expr(lowered_type),
                    branch_stack=branch_stack,
                )
            self.probe_expr(
                expr.body,
                local_entries + [(actual, lowered_type)],
                local_names + [actual],
                extra=extra,
                branch_stack=branch_stack,
            )
            return
        if isinstance(expr, EqExpr):
            self.probe_expr(expr.typ, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            self.probe_expr(expr.lhs, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            self.probe_expr(expr.rhs, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            return
        if isinstance(expr, ProductExpr):
            for arg in expr.args:
                self.probe_expr(arg, local_entries, local_names, extra=extra, branch_stack=branch_stack)
            return
        if isinstance(expr, MatchExpr):
            self.probe_match_like(expr, local_entries, local_names, extra=extra, is_case=False, branch_stack=branch_stack)
            return
        if isinstance(expr, CaseExpr):
            self.probe_match_like(expr, local_entries, local_names, extra=extra, is_case=True, branch_stack=branch_stack)
            return

    def probe_match_like(
        self,
        expr: MatchExpr | CaseExpr,
        local_entries: list[tuple[str, Expr]],
        local_names: list[str],
        *,
        extra: Optional[set[str]],
        is_case: bool,
        branch_stack: Optional[list[GuiBranchFrame]],
    ) -> None:
        family_name = expr.sum_type if isinstance(expr, CaseExpr) else expr.inductive
        spec = self.state.inductives.get(family_name)
        if spec is None:
            raise ElabError(f"unknown family in match/case: {family_name}")
        family_args = expr.type_args if isinstance(expr, CaseExpr) else expr.family_args
        for arg in family_args:
            self.probe_expr(arg, local_entries, local_names, extra=extra)
        self.probe_expr(expr.scrutinee, local_entries, local_names, extra=extra)

        bind_raw = list(expr.bind_names)
        if not bind_raw:
            bind_raw = [None] * (spec.param_count + spec.index_count)
        bind_actuals = [self.actual_binder_name(name, "b") for name in bind_raw]
        param_actuals = bind_actuals[: spec.param_count]
        index_actuals = bind_actuals[spec.param_count :]
        param_terms = [self.atom(name) for name in param_actuals]
        param_bindings = self.instantiate_telescope(spec.param_telescope, param_actuals)
        index_bindings = self.instantiate_index_telescope(spec, param_actuals, index_actuals)
        alias_actual = self.actual_binder_name(expr.alias, "q")
        motive_entries = local_entries + param_bindings + index_bindings
        motive_names = local_names + [name for name, _ in param_bindings] + [name for name, _ in index_bindings]
        motive_entries = motive_entries + [(alias_actual, self.atom(spec.name))]
        motive_names = motive_names + [alias_actual]
        self.probe_expr(expr.motive_body, motive_entries, motive_names, extra=extra, branch_stack=branch_stack)

        for branch, ctor_name in zip(expr.branches, spec.constructor_names):
            ctor = spec.constructors[ctor_name]
            field_actuals: list[str] = []
            field_types: list[tuple[str, Expr]] = []
            field_mapping = {
                orig_name: self.atom(actual)
                for (orig_name, _), actual in zip(spec.param_telescope, param_actuals)
            }
            for (orig_name, orig_type), raw_name in zip(ctor.fields, branch.fields):
                actual = self.actual_binder_name(raw_name, "f")
                actual_type = self.lower_substituted(orig_type, field_mapping)
                field_actuals.append(actual)
                field_types.append((actual, actual_type))
                field_mapping[orig_name] = self.atom(actual)
            ih_bindings: list[tuple[str, Expr]] = []
            if not is_case and isinstance(branch, MatchBranch):
                ih_raw = list(branch.ihs) or [None] * len(ctor.recursive_fields)
                motive_expr = self.lower_motive_expr(spec, param_actuals, index_actuals, alias_actual, expr.motive_body, local_names, extra)
                for rec, raw_ih in zip(ctor.recursive_fields, ih_raw):
                    actual = self.actual_binder_name(raw_ih, "ih")
                    ih_bindings.append(
                        (
                            actual,
                            self.build_instantiated_ih_type(
                                spec,
                                ctor,
                                rec,
                                param_terms,
                                field_actuals,
                                motive_expr,
                            ),
                        )
                    )
            param_entries = self.instantiate_telescope(spec.param_telescope, param_actuals)
            branch_entries = local_entries + param_entries + field_types + ih_bindings
            branch_names = local_names + [name for name, _ in param_entries] + field_actuals + [name for name, _ in ih_bindings]
            next_branch_stack = list(branch_stack or [])
            next_branch_stack.append(
                GuiBranchFrame(
                    kind="case" if is_case else "match",
                    constructor=ctor_name,
                    fields=list(field_actuals),
                    ihs=[name for name, _ in ih_bindings],
                )
            )
            if self.target_name_site is not None and self.target_name_site.owner_surface_id == branch.surface_id:
                if self.target_name_site.role == "branch_field" and self.target_name_site.index < len(field_types):
                    site_name, site_type = field_types[self.target_name_site.index]
                    self.capture_name_site(
                        self.target_name_site,
                        local_entries + param_entries + field_types[: self.target_name_site.index + 1],
                        resolved_name=f"binder:{site_name}",
                        lowered_expr=show_expr(site_type),
                        branch_stack=next_branch_stack,
                    )
                if self.target_name_site.role == "branch_ih" and self.target_name_site.index < len(ih_bindings):
                    site_name, site_type = ih_bindings[self.target_name_site.index]
                    self.capture_name_site(
                        self.target_name_site,
                        branch_entries[: len(local_entries) + len(param_entries) + len(field_types) + self.target_name_site.index + 1],
                        resolved_name=f"binder:{site_name}",
                        lowered_expr=show_expr(site_type),
                        branch_stack=next_branch_stack,
                    )
            self.probe_expr(branch.body, branch_entries, branch_names, extra=extra, branch_stack=next_branch_stack)

    def lower_substituted(self, expr: Expr, mapping: dict[str, Expr]) -> Expr:
        from .restricted import substitute_expr

        return substitute_expr(
            expr,
            mapping,
            fresh_name=self.state.fresh,
            allocator=self.allocate_surface,
        )

    def lower_motive_expr(
        self,
        spec,
        param_actuals: list[str],
        index_actuals: list[str],
        alias_actual: str,
        motive_body: Expr,
        local_names: list[str],
        extra: Optional[set[str]],
    ) -> Expr:
        param_bindings = self.instantiate_telescope(spec.param_telescope, param_actuals)
        index_bindings = self.instantiate_index_telescope(spec, param_actuals, index_actuals)
        motive_local = ElabLocalContext(local_names + [name for name, _ in param_bindings] + [name for name, _ in index_bindings] + [alias_actual])
        motive_body_lowered = super().lower_expr(motive_body, motive_local, extra=extra)
        return fold_lam_expr(
            param_bindings + index_bindings + [(alias_actual, self.atom(spec.name))],
            motive_body_lowered,
            self.allocate_surface,
        )


def analyze_source(
    source: str,
    cursor_pos: int,
    *,
    compile_mode: bool = False,
    target_override_id: Optional[int] = None,
) -> GuiAnalysis:
    try:
        tokens = tokenize(source)
        artifacts = parse_with_metadata(tokens)
        focus = cursor_focus(tokens, cursor_pos)
        decl = decl_for_focus(artifacts.decls, artifacts.node_spans, focus)
        decl_name_sites = [] if decl is None else [site for site in artifacts.name_sites if span_contains_focus(artifacts.node_spans.get(decl.surface_id, (-1, -1)), TokenFocus(site.token_span[0]))]
        target_name_site = select_name_site_for_focus(decl_name_sites, focus)
        minimal_expr = None if decl is None else select_expr_for_focus(decl, artifacts.node_spans, focus)
        target_expr = None if target_name_site is not None else minimal_expr
        target_parent_id = None
        if decl is not None:
            parent_map = expr_parent_map(decl)
            if target_override_id is not None:
                override_expr = find_expr_by_id(decl, target_override_id)
                if override_expr is not None:
                    target_expr = override_expr
                    target_name_site = None
            elif target_name_site is not None:
                target_parent_id = minimal_expr.surface_id if minimal_expr is not None else target_name_site.parent_expr_id
            if target_expr is not None:
                target_parent_id = parent_map.get(target_expr.surface_id)
        if target_name_site is not None and target_expr is None:
            target_span = target_name_site.token_span
        else:
            target_span = None if target_expr is None else artifacts.node_spans.get(target_expr.surface_id)
        target_offsets = None if target_span is None else token_span_to_offsets(tokens, target_span)
        probe = None
        if decl is not None and (target_expr is not None or target_name_site is not None):
            probe = GuiProbeElaborator(
                None if target_expr is None else target_expr.surface_id,
                target_name_site,
            ).probe_program(artifacts.decls, decl)
        compile_status = None
        proof_nodes = None
        if compile_mode:
            try:
                from .pipeline import run_pipeline_detailed
                from .proof_visualizer import ProofTreeBuilder, theorem_param_counts_from_decls

                _, _, _, core_decls, global_ctx = run_pipeline_detailed(source)
                compile_status = "typecheck: OK"
                param_counts = theorem_param_counts_from_decls(artifacts.decls)
                proof_nodes = ProofTreeBuilder(core_decls, global_ctx, param_counts).build()
            except (TinyCheckerError, ParseError, ElabError, TypeCheckError) as exc:
                compile_status = f"typecheck error: {exc}"
        return GuiAnalysis(
            source=source,
            tokens=tokens,
            artifacts=artifacts,
            focus=focus,
            decl=decl,
            target_expr=target_expr,
            target_name_site=target_name_site,
            target_span=target_span,
            target_offsets=target_offsets,
            probe=probe,
            minimal_expr=minimal_expr,
            target_parent_id=target_parent_id,
            compile_status=compile_status,
            proof_nodes=proof_nodes,
            error=None,
        )
    except (TinyCheckerError, ParseError, ElabError, TypeCheckError) as exc:
        return GuiAnalysis(
            source=source,
            tokens=[],
            artifacts=ParseArtifacts([], {}, []),
            focus=GapFocus(0),
            decl=None,
            target_expr=None,
            target_name_site=None,
            target_span=None,
            target_offsets=None,
            probe=None,
            minimal_expr=None,
            target_parent_id=None,
            compile_status=None,
            error=str(exc),
        )
