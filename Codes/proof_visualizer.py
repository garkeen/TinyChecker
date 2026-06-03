from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from .core import CApp, CDecl, CDefinition, CGlobal, CLam, CPi, CTerm, CType, CVar, mk_apps, unfold_app
from .core_ops import instantiate
from .errors import TinyCheckerError
from .lexer import tokenize
from .parser import parse
from .pipeline import run_pipeline_detailed
from .reducer import Reducer
from .runtime import EliminatorInfo, TcGlobalContext
from .surface import Decl, FunDecl
from .typechecker import TcContext, TypeChecker, extend_context


LOGIC_TYPE_NAMES = {"Eq", "Or", "And", "Exists", "Empty", "empty", "Unit"}


@dataclass
class ProofNode:
    title: str
    conclusion: Optional[str] = None
    children: list["ProofNode"] = field(default_factory=list)

    def render(self, indent: int = 0) -> list[str]:
        prefix = "  " * indent
        if self.title == "":
            lines: list[str] = []
            for child in self.children:
                lines.extend(child.render(indent))
            return lines
        if self.conclusion is None:
            lines = [f"{prefix}{self.title}"]
        else:
            separator = "：" if self.title.endswith("证明") else ": "
            lines = [f"{prefix}{self.title}{separator}{self.conclusion}"]
        for child in self.children:
            lines.extend(child.render(indent + 1))
        return lines


@dataclass
class NameSupply:
    prefix: tuple[int, ...] = ()
    next_index: int = 1

    def fresh_path(self) -> tuple[int, ...]:
        path = self.prefix + (self.next_index,)
        self.next_index += 1
        return path

    @staticmethod
    def label(path: tuple[int, ...]) -> str:
        return "引理 " + ".".join(str(part) for part in path)

    def child(self, path: tuple[int, ...]) -> "NameSupply":
        return NameSupply(prefix=path)


class LogicPretty:
    def text(self, term: CTerm, names: tuple[Optional[str], ...] = ()) -> str:
        return self._term(beta_nf(term), names, 0)

    def _term(self, term: CTerm, names: tuple[Optional[str], ...], prec: int) -> str:
        head, args = unfold_app(term)
        if isinstance(head, CGlobal):
            if head.name in {"Empty", "empty"} and not args:
                return "⊥"
            if head.name in {"Unit", "unit_type"} and not args:
                return "⊤"
            if head.name == "Or" and len(args) == 2:
                text = f"{self._term(args[0], names, 2)} ∨ {self._term(args[1], names, 2)}"
                return self._par(text, prec > 1)
            if head.name == "And" and len(args) == 2:
                text = f"{self._term(args[0], names, 2)} ∧ {self._term(args[1], names, 2)}"
                return self._par(text, prec > 1)
            if head.name == "Eq" and len(args) == 3:
                dom = self._term(args[0], names, 0)
                text = f"[{dom}] {self._term(args[1], names, 1)} = {self._term(args[2], names, 1)}"
                return self._par(text, prec > 0)
            if head.name == "Exists" and len(args) == 2:
                text = self._exists_chain_text(term, names)
                return self._par(text, prec > 0)

        if isinstance(term, CPi):
            text = self._pi_chain(term, names)
            return self._par(text, prec > 0)
        if isinstance(term, CLam):
            eta = eta_contract(term)
            if eta is not None:
                return self._term(eta, names, prec)
            body = self._term(term.body, (term.name,) + names, 0)
            return f"\\({term.name}:{self._term(term.param_type, names, 0)}) => {body}"
        if isinstance(term, CType):
            return "Type"
        if isinstance(term, CGlobal):
            return term.name
        if isinstance(term, CVar):
            return self._lookup(term.index, names, term.name)
        if isinstance(term, CApp):
            text = self._app(term, names)
            return self._par(text, prec > 2)
        return repr(term)

    def _app(self, term: CApp, names: tuple[Optional[str], ...]) -> str:
        head, args = unfold_app(term)
        if isinstance(head, CGlobal):
            pieces = [head.name]
        elif isinstance(head, CVar):
            pieces = [self._lookup(head.index, names, head.name)]
        else:
            pieces = [f"({self._term(head, names, 0)})"]
        for arg in args:
            text = self._term(arg, names, 0)
            if not isinstance(arg, CType | CGlobal | CVar):
                text = f"({text})"
            pieces.append(text)
        return " ".join(pieces)

    def app_arg_text(self, term: CTerm, names: tuple[Optional[str], ...] = ()) -> str:
        text = self.text(term, names)
        return text if self._is_atomic_app_arg(term) else f"({text})"

    def _is_atomic_app_arg(self, term: CTerm) -> bool:
        return isinstance(beta_nf(term), CType | CGlobal | CVar)

    def _pi_chain(self, term: CPi, names: tuple[Optional[str], ...]) -> str:
        parts: list[tuple[str, str]] = []
        named_group: list[str] = []
        named_type: Optional[str] = None
        current: CTerm = term
        current_names = names

        def flush_named() -> None:
            nonlocal named_group, named_type
            if named_group and named_type is not None:
                parts.append(("forall", f"{' '.join(named_group)} : {named_type}"))
            named_group = []
            named_type = None

        while isinstance(current, CPi):
            domain_text = self._term(current.domain, current_names, 1)
            if is_anonymous_name(current.name):
                flush_named()
                parts.append(("arrow", self._pi_side(domain_text)))
                current_names = (None,) + current_names
            else:
                if named_group and named_type == domain_text:
                    named_group.append(current.name)
                else:
                    flush_named()
                    named_group = [current.name]
                    named_type = domain_text
                current_names = (current.name,) + current_names
            current = current.codomain
        flush_named()
        body = self._term(current, current_names, 0)
        return self._render_pi_parts(parts, body)

    def _render_pi_parts(self, parts: list[tuple[str, str]], body: str) -> str:
        result = body
        for kind, text in reversed(parts):
            if kind == "forall":
                result = f"∀ {text}，{result}"
            else:
                result = f"{text} -> {result}"
        return result

    def _lookup(self, index: int, names: tuple[Optional[str], ...], hint: Optional[str]) -> str:
        if index < len(names) and names[index]:
            return str(names[index])
        return hint or f"@{index}"

    def _exists_chain_text(self, term: CTerm, names: tuple[Optional[str], ...]) -> str:
        quantifiers, body, body_names = self._peel_exists_chain(term, names)
        if not quantifiers:
            return self._term(term, names, 0)
        quant_text = "，".join(f"∃ {name} : {self._term(dom, env_names, 0)}" for name, dom, env_names in quantifiers)
        return f"{quant_text}，{self._term(body, body_names, 0)}"

    def _peel_exists_chain(
        self,
        term: CTerm,
        names: tuple[Optional[str], ...],
    ) -> tuple[list[tuple[str, CTerm, tuple[Optional[str], ...]]], CTerm, tuple[Optional[str], ...]]:
        quantifiers: list[tuple[str, CTerm, tuple[Optional[str], ...]]] = []
        current = beta_nf(term)
        current_names = names
        used_names: set[str] = {name for name in names if name}
        while True:
            head, args = unfold_app(current)
            if not (isinstance(head, CGlobal) and head.name == "Exists" and len(args) == 2):
                return quantifiers, current, current_names
            dom = args[0]
            pred = beta_nf(args[1])
            if not isinstance(pred, CLam):
                witness = self._pick_exists_name("x", used_names)
                quantifiers.append((witness, dom, current_names))
                return quantifiers, CApp(pred, CVar(0, witness)), (witness,) + current_names
            witness = self._pick_exists_name(pred.name if not is_anonymous_name(pred.name) else "x", used_names)
            used_names.add(witness)
            quantifiers.append((witness, dom, current_names))
            current_names = (witness,) + current_names
            current = pred.body

    def _pick_exists_name(self, base: str, used_names: set[str]) -> str:
        candidate = base if base and not is_anonymous_name(base) else "x"
        if candidate not in used_names:
            return candidate
        index = 1
        while f"{candidate}{index}" in used_names:
            index += 1
        return f"{candidate}{index}"

    def _pi_side(self, text: str) -> str:
        return text

    def _par(self, text: str, need: bool) -> str:
        return f"({text})" if need else text


class ProofTreeBuilder:
    LONG_ARG_NODE_THRESHOLD = 12

    def __init__(
        self,
        core_decls: list[CDecl],
        global_ctx: TcGlobalContext,
        theorem_param_counts: dict[str, int],
    ) -> None:
        self.core_decls = core_decls
        self.global_ctx = global_ctx
        self.theorem_param_counts = theorem_param_counts
        self.pretty = LogicPretty()
        self.reducer = Reducer(global_ctx)
        self.checker = TypeChecker()
        self.checker.global_ctx = global_ctx

    def build(self) -> list[ProofNode]:
        nodes: list[ProofNode] = []
        for decl in self.core_decls:
            if not isinstance(decl, CDefinition):
                continue
            if decl.kind not in {"theorem", "claim"} or decl.value is None:
                continue
            nodes.append(self.definition_node(decl))
        return nodes

    def definition_node(self, decl: CDefinition) -> ProofNode:
        ctx = TcContext()
        body = decl.value
        intro_binders: list[tuple[str, CTerm, tuple[Optional[str], ...]]] = []
        if decl.kind == "theorem":
            body, ctx, intro_binders = self.peel_n_lambdas(
                body,
                ctx,
                self.theorem_param_counts.get(decl.name, 0),
            )
        label = "定理" if decl.kind == "theorem" else "断言"
        proof = self.narrate(body, ctx, NameSupply())
        if intro_binders:
            proof = ProofNode(
                f"引入 {self.binders_text(intro_binders)}，目标",
                conclusion=self.type_text(body, ctx),
                children=[proof],
            )
        children = [proof]
        return ProofNode(
            f"{label} {decl.name}",
            conclusion=self.sequent_text(decl.typ, ()),
            children=children,
        )

    def narrate(self, term: CTerm, ctx: TcContext, supply: NameSupply) -> ProofNode:
        exposed = expose_lambda(term)
        if isinstance(exposed, CLam):
            eta = eta_contract(exposed)
            if eta is not None:
                return self.narrate(eta, ctx, supply)
            return self.lambda_node(exposed, ctx, supply)

        head, args = unfold_app(term)
        if isinstance(head, CGlobal):
            proj = self.projection_view(head.name, args)
            if proj is not None:
                owner, field_name = proj
                return self.projection_application_node(owner, field_name, term, args, ctx, supply)
            elim = self.global_ctx.eliminators.get(head.name)
            if elim is not None:
                branch_ready = self.branch_ready_arity(elim)
                if len(args) >= branch_ready:
                    return self.eliminator_node(elim, args, ctx, supply)

        if isinstance(term, CApp):
            return self.application_node(head, args, ctx, supply)
        return self.atomic_node(term, ctx)

    def lambda_node(self, term: CLam, ctx: TcContext, supply: NameSupply) -> ProofNode:
        binders, body, body_ctx = self.peel_lambdas(term, ctx)
        intro = self.binders_text(binders)
        return ProofNode(
            f"引入 {intro}，目标",
            conclusion=self.type_text(body, body_ctx),
            children=[self.narrate(body, body_ctx, supply)],
        )

    def eliminator_node(self, elim: EliminatorInfo, args: list[CTerm], ctx: TcContext, supply: NameSupply) -> ProofNode:
        branch_ready = self.branch_ready_arity(elim)
        base_args = args[:branch_ready]
        rest = args[branch_ready:]
        base = mk_apps(CGlobal(elim.name), base_args)
        generic = self.eliminator_generic_node(elim, base_args, ctx, supply)
        if not rest:
            return generic

        ind = self.global_ctx.inductives[elim.owner]
        params = base_args[: ind.param_count]
        indices = rest[: ind.index_count]
        scrutinee = rest[ind.index_count : ind.index_count + 1]
        extra = rest[ind.index_count + 1 :]
        pieces: list[str] = []
        if params:
            pieces.append("参数 " + ", ".join(self.term_ref(arg, ctx) for arg in params))
        if indices:
            pieces.append("索引 " + ", ".join(self.term_ref(arg, ctx) for arg in indices))
        if scrutinee:
            pieces.append("对象 " + self.term_ref(scrutinee[0], ctx))
        if extra:
            pieces.append("额外参数 " + ", ".join(self.term_ref(arg, ctx) for arg in extra))
        if not pieces:
            pieces.append("若干参数")
        return ProofNode(
            "",
            children=[
                generic,
                ProofNode(
                    "由上述通用消去证明，实例化到" + "，".join(pieces) + "，得到",
                    conclusion=self.type_text(mk_apps(base, rest), ctx),
                ),
            ],
        )

    def eliminator_generic_node(self, elim: EliminatorInfo, args: list[CTerm], ctx: TcContext, supply: NameSupply) -> ProofNode:
        owner = elim.owner
        ind = self.global_ctx.inductives[owner]
        motive = args[ind.param_count]
        branches = args[ind.param_count + 1 : ind.param_count + 1 + len(elim.branch_order)]
        owner_text = self.pretty.text(CGlobal(owner), tuple(ctx.names))
        if owner == "Exists":
            title = f"对{owner_text}应用存在消去（∃E），证明"
        elif self.eliminator_has_ih(elim):
            title = f"对{owner_text}利用结构归纳法，证明"
        else:
            title = f"对{owner_text}进行分类讨论，证明"
        children: list[ProofNode] = []
        for ctor_name, branch in zip(elim.branch_order, branches):
            children.append(self.branch_node(ctor_name, branch, ctx, supply))
        return ProofNode(
            title,
            conclusion=self.generic_eliminator_goal(motive, elim, args, ctx),
            children=children,
        )

    def branch_node(self, ctor_name: str, branch: CTerm, ctx: TcContext, supply: NameSupply) -> ProofNode:
        binders, body, body_ctx = self.peel_lambdas(branch, ctx)
        intro = self.binders_text(binders)
        title = f"分支 {ctor_name}"
        if intro != "无额外假设":
            title += f"：引入 {intro}，目标"
        else:
            title += "，目标"
        return ProofNode(
            title,
            conclusion=self.type_text(body, body_ctx),
            children=[self.narrate(body, body_ctx, supply)],
        )

    def application_node(self, head: CTerm, args: list[CTerm], ctx: TcContext, supply: NameSupply) -> ProofNode:
        try:
            head_type = self.infer(head, ctx)
        except TinyCheckerError:
            head_type = CType()
        segments: list[ProofNode] = []
        current_head = head
        current_ref = self.term_ref(head, ctx)
        current_type = head_type
        remaining = list(args)
        current_is_ctor = self.is_constructor_head(head)
        product_owner = self.product_constructor_owner(head)
        product_ctor_step = product_owner is not None

        while remaining:
            count = self.plan_segment(current_type, remaining)
            if count <= 0:
                count = 1
            used = remaining[:count]
            proof_refs: list[str] = []
            app_refs: list[str] = []
            lemma_children: list[ProofNode] = []
            result_type = current_type
            for arg in used:
                pi = self.reducer.whnf(result_type)
                expected: Optional[CTerm] = None
                if isinstance(pi, CPi):
                    expected = pi.domain
                    result_type = instantiate(pi.codomain, arg)
                else:
                    result_type = CType()
                ref, lemma = self.argument_ref(arg, ctx, supply, expected)
                proof_refs.append(ref)
                app_refs.append(ref if lemma is not None else self.pretty.app_arg_text(arg, tuple(ctx.names)))
                if lemma is not None:
                    lemma_children.append(lemma)
            result_text = self.pretty.text(self.reducer.whnf(result_type), tuple(ctx.names))
            if current_is_ctor:
                ctor_app = " ".join([current_ref, *app_refs])
                if isinstance(current_head, CGlobal):
                    ctor_info = self.global_ctx.constructors.get(current_head.name)
                    if ctor_info is not None and ctor_info.owner == "Exists":
                        title = f"给出存在见证（∃I） {ctor_app}"
                    elif product_ctor_step and product_owner is not None:
                        if self.product_is_sigma_like(product_owner):
                            title = f"给出存在证据 {ctor_app}"
                        else:
                            title = f"构造配对证据 {ctor_app}"
                    else:
                        title = f"构造 {result_text} 实例 {ctor_app}"
                elif product_ctor_step and product_owner is not None:
                    if self.product_is_sigma_like(product_owner):
                        title = f"给出存在证据 {ctor_app}"
                    else:
                        title = f"构造配对证据 {ctor_app}"
                else:
                    title = f"构造 {result_text} 实例 {ctor_app}"
            elif proof_refs:
                title = f"由 {', '.join(proof_refs)}，通过 {current_ref} 推导即得"
            else:
                title = f"通过 {current_ref} 推导即得"
            step = ProofNode(
                title,
                conclusion=None if current_is_ctor else result_text,
            )
            if lemma_children:
                segments.extend(lemma_children)
                segments.append(step)
            else:
                segments.append(step)
            current_head = mk_apps(current_head, used)
            current_ref = "上一步结果"
            current_type = result_type
            current_is_ctor = False
            product_ctor_step = False
            remaining = remaining[count:]

        if len(segments) == 1:
            return segments[0]
        return ProofNode(
            "",
            children=segments,
        )

    def atomic_node(self, term: CTerm, ctx: TcContext) -> ProofNode:
        if isinstance(term, CGlobal):
            entry = self.global_ctx.globals.get(term.name)
            kind = entry.kind if entry is not None else "global"
            if kind == "data_ctor":
                return ProofNode(f"构造 {self.type_text(term, ctx)} 实例 {self.term_ref(term, ctx)}")
            return ProofNode(f"引用 {term.name} [{kind}]", conclusion=self.type_text(term, ctx))
        if isinstance(term, CVar):
            return ProofNode(f"使用 {self.term_ref(term, ctx)}", conclusion=self.type_text(term, ctx))
        return ProofNode(f"直接给出 {self.term_ref(term, ctx)}", conclusion=self.type_text(term, ctx))

    def argument_ref(
        self,
        term: CTerm,
        ctx: TcContext,
        supply: NameSupply,
        expected: Optional[CTerm],
    ) -> tuple[str, Optional[ProofNode]]:
        if not self.should_extract_lemma(term, ctx, expected):
            return self.term_ref(term, ctx), None
        if self.is_type_argument(term, ctx):
            return self.term_ref(term, ctx), None
        path = supply.fresh_path()
        name = NameSupply.label(path)
        proof = self.narrate(term, ctx, supply.child(path))
        return name, ProofNode(name, conclusion=self.type_text(term, ctx), children=[proof])

    def is_type_argument(self, term: CTerm, ctx: TcContext) -> bool:
        try:
            return isinstance(self.reducer.whnf(self.infer(term, ctx)), CType)
        except Exception:
            return False

    def should_extract_lemma(self, term: CTerm, ctx: TcContext, expected: Optional[CTerm]) -> bool:
        if isinstance(term, CType | CGlobal | CVar):
            return False
        if expected is not None and isinstance(self.reducer.whnf(expected), CType):
            return False
        if self.is_theorem_like_application(term):
            return self.term_size(term) > 4
        if isinstance(term, CLam):
            return self.has_logical_result(term, ctx)
        if self.contains_branch_ready_elim(term):
            return True
        return self.term_size(term) > self.LONG_ARG_NODE_THRESHOLD and self.has_logical_result(term, ctx)

    def is_theorem_like_application(self, term: CTerm) -> bool:
        head, _args = unfold_app(term)
        if not isinstance(head, CGlobal):
            return False
        entry = self.global_ctx.globals.get(head.name)
        return entry is not None and entry.kind in {"theorem", "claim", "axiom"}

    def has_logical_result(self, term: CTerm, ctx: TcContext) -> bool:
        try:
            return self.is_logical_type(self.infer(term, ctx))
        except Exception:
            return False

    def is_logical_type(self, typ: CTerm) -> bool:
        wh = self.reducer.whnf(typ)
        if isinstance(wh, CPi):
            return self.is_logical_type(wh.codomain)
        head, _args = unfold_app(wh)
        return isinstance(head, CGlobal) and head.name in LOGIC_TYPE_NAMES

    def contains_branch_ready_elim(self, term: CTerm) -> bool:
        head, args = unfold_app(term)
        if isinstance(head, CGlobal):
            elim = self.global_ctx.eliminators.get(head.name)
            if elim is not None and len(args) >= self.branch_ready_arity(elim):
                return True
        if isinstance(term, CApp):
            return self.contains_branch_ready_elim(term.func) or self.contains_branch_ready_elim(term.arg)
        if isinstance(term, CPi):
            return self.contains_branch_ready_elim(term.domain) or self.contains_branch_ready_elim(term.codomain)
        if isinstance(term, CLam):
            return self.contains_branch_ready_elim(term.param_type) or self.contains_branch_ready_elim(term.body)
        return False

    def plan_segment(self, typ: CTerm, args: list[CTerm]) -> int:
        count = 0
        current = typ
        while count < len(args):
            wh = self.reducer.whnf(current)
            if not isinstance(wh, CPi):
                break
            current = instantiate(wh.codomain, args[count])
            count += 1
        return count

    def peel_n_lambdas(
        self,
        term: CTerm,
        ctx: TcContext,
        count: int,
    ) -> tuple[CTerm, TcContext, list[tuple[str, CTerm, tuple[Optional[str], ...]]]]:
        binders: list[tuple[str, CTerm, tuple[Optional[str], ...]]] = []
        current = term
        current_ctx = ctx
        for _ in range(count):
            exposed = expose_lambda(current)
            if not isinstance(exposed, CLam):
                break
            binders.append((exposed.name, exposed.param_type, tuple(current_ctx.names)))
            current_ctx = extend_context(current_ctx, exposed.name, exposed.param_type)
            current = exposed.body
        return current, current_ctx, binders

    def peel_lambdas(self, term: CTerm, ctx: TcContext) -> tuple[list[tuple[str, CTerm, tuple[Optional[str], ...]]], CTerm, TcContext]:
        binders: list[tuple[str, CTerm, tuple[Optional[str], ...]]] = []
        current = term
        current_ctx = ctx
        while True:
            exposed = expose_lambda(current)
            if not isinstance(exposed, CLam):
                break
            binders.append((exposed.name, exposed.param_type, tuple(current_ctx.names)))
            current_ctx = extend_context(current_ctx, exposed.name, exposed.param_type)
            current = exposed.body
        return binders, current, current_ctx

    def binders_text(self, binders: list[tuple[str, CTerm, tuple[Optional[str], ...]]]) -> str:
        if not binders:
            return "无额外假设"
        groups: list[tuple[list[str], str]] = []
        for name, typ, names in binders:
            typ_text = self.pretty.text(self.reducer.whnf(typ), names)
            if groups and groups[-1][1] == typ_text:
                groups[-1][0].append(name)
            else:
                groups.append(([name], typ_text))
        return "，".join(f"{' '.join(names)} : {typ}" for names, typ in groups)

    def branch_ready_arity(self, elim: EliminatorInfo) -> int:
        ind = self.global_ctx.inductives[elim.owner]
        return ind.param_count + 1 + len(elim.branch_order)

    def eliminator_has_ih(self, elim: EliminatorInfo) -> bool:
        for ctor_name in elim.branch_order:
            ctor = self.global_ctx.constructors.get(ctor_name)
            if ctor is not None and ctor.recursive_fields:
                return True
        return False

    def generic_eliminator_goal(
        self,
        motive: CTerm,
        elim: EliminatorInfo,
        args: list[CTerm],
        ctx: TcContext,
    ) -> str:
        motive_goal = self.motive_as_pi(motive)
        if motive_goal is not None:
            return self.sequent_text(motive_goal, tuple(ctx.names))
        return self.sequent_text(self.infer(mk_apps(CGlobal(elim.name), args), ctx), tuple(ctx.names))

    def motive_as_pi(self, motive: CTerm) -> Optional[CTerm]:
        exposed = expose_lambda(motive)
        if not isinstance(exposed, CLam):
            return None
        return self.lambda_chain_as_pi(exposed)

    def lambda_chain_as_pi(self, term: CLam) -> CPi:
        body = expose_lambda(term.body)
        if isinstance(body, CLam):
            codomain: CTerm = self.lambda_chain_as_pi(body)
        else:
            codomain = term.body
        return CPi(term.name, term.param_type, codomain)

    def infer(self, term: CTerm, ctx: TcContext) -> CTerm:
        return self.checker.infer(term, ctx)

    def type_text(self, term: CTerm, ctx: TcContext) -> str:
        try:
            typ = self.reducer.whnf(self.infer(term, ctx))
            return self.pretty.text(typ, tuple(ctx.names))
        except Exception:
            return "?"

    def sequent_text(self, typ: CTerm, names: tuple[Optional[str], ...]) -> str:
        pieces: list[str] = []
        current = typ
        current_names = names
        named_group: list[str] = []
        named_type: Optional[str] = None

        def flush_named() -> None:
            nonlocal named_group, named_type
            if named_group and named_type is not None:
                pieces.append(f"∀ {' '.join(named_group)} : {named_type}")
            named_group = []
            named_type = None

        while isinstance(current, CPi):
            domain = self.pretty.text(current.domain, current_names)
            if is_anonymous_name(current.name):
                flush_named()
                pieces.append(domain)
                current_names = (None,) + current_names
            else:
                if named_group and named_type == domain:
                    named_group.append(current.name)
                else:
                    flush_named()
                    named_group = [current.name]
                    named_type = domain
                current_names = (current.name,) + current_names
            current = current.codomain
        flush_named()
        goal = self.pretty.text(current, current_names)
        return f"{'，'.join(pieces)} |- {goal}" if pieces else goal

    def term_ref(self, term: CTerm, ctx: TcContext) -> str:
        return self.pretty.text(term, tuple(ctx.names))

    def is_constructor_head(self, head: CTerm) -> bool:
        return isinstance(head, CGlobal) and head.name in self.global_ctx.constructors

    def product_constructor_owner(self, head: CTerm) -> Optional[str]:
        if not isinstance(head, CGlobal):
            return None
        ctor = self.global_ctx.constructors.get(head.name)
        if ctor is None:
            return None
        ind = self.global_ctx.inductives.get(ctor.owner)
        if ind is None or ind.kind != "product":
            return None
        return ctor.owner

    def projection_view(self, head_name: str, args: list[CTerm]) -> Optional[tuple[str, str]]:
        if not args:
            return None
        entry = self.global_ctx.globals.get(head_name)
        if entry is None or entry.kind != "projection":
            return None
        if "." not in head_name:
            return None
        owner, field_name = head_name.split(".", 1)
        ind = self.global_ctx.inductives.get(owner)
        if ind is None or ind.kind != "product":
            return None
        return owner, field_name

    def projection_application_node(
        self,
        owner: str,
        field_name: str,
        term: CTerm,
        args: list[CTerm],
        ctx: TcContext,
        supply: NameSupply,
    ) -> ProofNode:
        if len(args) != 1:
            head, full_args = unfold_app(term)
            return self.application_node(head, full_args, ctx, supply)
        scrutinee = args[0]
        expected: Optional[CTerm] = None
        try:
            head_type = self.infer(CGlobal(f"{owner}.{field_name}"), ctx)
            wh = self.reducer.whnf(head_type)
            if isinstance(wh, CPi):
                expected = wh.domain
        except Exception:
            expected = None
        ref, lemma = self.argument_ref(scrutinee, ctx, supply, expected)
        if self.product_is_sigma_like(owner):
            title = f"对存在证据 {ref} 做消去，取出分量 {field_name}"
        else:
            title = f"对配对证据 {ref} 取投影 {field_name}"
        step = ProofNode(title, conclusion=self.type_text(term, ctx))
        if lemma is None:
            return step
        return ProofNode("", children=[lemma, step])

    def product_is_sigma_like(self, owner: str) -> bool:
        ind = self.global_ctx.inductives.get(owner)
        if ind is None or ind.kind != "product" or not ind.constructor_names:
            return False
        ctor_name = ind.constructor_names[0]
        ctor = self.global_ctx.constructors.get(ctor_name)
        if ctor is None:
            return False
        field_types = [typ for _, typ in ctor.fields[ctor.param_count:]]
        for index, field_type in enumerate(field_types):
            if index == 0:
                continue
            if self.type_mentions_previous_fields(field_type, index):
                return True
        return False

    def type_mentions_previous_fields(self, term: CTerm, prev_count: int, depth: int = 0) -> bool:
        if isinstance(term, CVar):
            return depth <= term.index < depth + prev_count
        if isinstance(term, CApp):
            return self.type_mentions_previous_fields(term.func, prev_count, depth) or self.type_mentions_previous_fields(
                term.arg, prev_count, depth,
            )
        if isinstance(term, CPi):
            return self.type_mentions_previous_fields(term.domain, prev_count, depth) or self.type_mentions_previous_fields(
                term.codomain, prev_count, depth + 1,
            )
        if isinstance(term, CLam):
            return self.type_mentions_previous_fields(term.param_type, prev_count, depth) or self.type_mentions_previous_fields(
                term.body, prev_count, depth + 1,
            )
        return False

    def term_size(self, term: CTerm) -> int:
        if isinstance(term, CType | CGlobal | CVar):
            return 1
        if isinstance(term, CApp):
            return 1 + self.term_size(term.func) + self.term_size(term.arg)
        if isinstance(term, CPi):
            return 1 + self.term_size(term.domain) + self.term_size(term.codomain)
        if isinstance(term, CLam):
            return 1 + self.term_size(term.param_type) + self.term_size(term.body)
        return 1


def expose_lambda(term: CTerm) -> CTerm:
    current = term
    while True:
        if isinstance(current, CLam):
            return current
        head, args = unfold_app(current)
        if isinstance(head, CLam) and args:
            current = mk_apps(instantiate(head.body, args[0]), args[1:])
            continue
        return current


def eta_contract(term: CLam) -> Optional[CTerm]:
    body = expose_lambda(term.body)
    if isinstance(body, CLam):
        return None
    head, args = unfold_app(body)
    if not args:
        return None
    last = args[-1]
    if not (isinstance(last, CVar) and last.index == 0):
        return None
    candidate = mk_apps(head, args[:-1])
    if contains_var(candidate, 0):
        return None
    return drop_binder(candidate, 0)


def contains_var(term: CTerm, index: int) -> bool:
    if isinstance(term, CVar):
        return term.index == index
    if isinstance(term, CApp):
        return contains_var(term.func, index) or contains_var(term.arg, index)
    if isinstance(term, CPi):
        return contains_var(term.domain, index) or contains_var(term.codomain, index + 1)
    if isinstance(term, CLam):
        return contains_var(term.param_type, index) or contains_var(term.body, index + 1)
    return False


def drop_binder(term: CTerm, cutoff: int) -> CTerm:
    if isinstance(term, CVar):
        if term.index > cutoff:
            return CVar(term.index - 1, term.name)
        return term
    if isinstance(term, CApp):
        return CApp(drop_binder(term.func, cutoff), drop_binder(term.arg, cutoff))
    if isinstance(term, CPi):
        return CPi(term.name, drop_binder(term.domain, cutoff), drop_binder(term.codomain, cutoff + 1))
    if isinstance(term, CLam):
        return CLam(term.name, drop_binder(term.param_type, cutoff), drop_binder(term.body, cutoff + 1))
    return term


def beta_nf(term: CTerm) -> CTerm:
    if isinstance(term, CApp):
        func = beta_nf(term.func)
        arg = beta_nf(term.arg)
        if isinstance(func, CLam):
            return beta_nf(instantiate(func.body, arg))
        return CApp(func, arg)
    if isinstance(term, CPi):
        return CPi(term.name, beta_nf(term.domain), beta_nf(term.codomain))
    if isinstance(term, CLam):
        return CLam(term.name, beta_nf(term.param_type), beta_nf(term.body))
    return term


def is_anonymous_name(name: str) -> bool:
    return name == "_" or name.startswith("_")


def theorem_param_counts_from_decls(decls: list[Decl]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for decl in decls:
        if isinstance(decl, FunDecl) and decl.kind == "theorem":
            counts[decl.name] = len(decl.params)
    return counts


def build_proof_forest(source: str) -> list[ProofNode]:
    tokens = tokenize(source)
    decls = parse(tokens)
    theorem_param_counts = theorem_param_counts_from_decls(decls)
    _tokens, _decls, _restricted, core_decls, global_ctx = run_pipeline_detailed(source)
    return ProofTreeBuilder(core_decls, global_ctx, theorem_param_counts).build()


def render_proof_forest(source: str, title: str = "<source>") -> str:
    nodes = build_proof_forest(source)
    lines = [f"文件：{title}"]
    for node in nodes:
        lines.append("")
        lines.extend(node.render())
    return "\n".join(lines).rstrip() + "\n"


def render_file(path: str | Path) -> str:
    file_path = Path(path)
    return render_proof_forest(file_path.read_text(encoding="utf-8"), file_path.as_posix())


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate TinyChecker proof trees as plain text.")
    parser.add_argument("path", help="TinyChecker source file")
    parser.add_argument("--out", default="proof_visualizer.txt", help="output text file")
    args = parser.parse_args()
    output = render_file(args.path)
    Path(args.out).write_text(output, encoding="utf-8")
    print(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
