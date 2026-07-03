from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .errors import ElabError
from .restricted import (
    RDataCtorDecl,
    RDecl,
    RDefinition,
    REliminatorDecl,
    RRecFieldInfo,
    RTypeCtorDecl,
    contains_atom,
    fold_lam_expr,
    fold_pi_expr,
    restricted_decl_to_core,
    split_pi_expr,
    substitute_expr,
)
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
    mk_surface_apps,
    unfold_surface_app,
)


@dataclass
class ElabLocalContext:
    names: list[str] = field(default_factory=list)


@dataclass(frozen=True)
class ElabRecFieldInfo:
    field_position: int
    recursive_kind: str
    ho_telescope: list[tuple[str, Expr]]
    target_index_exprs: list[Expr]


@dataclass(frozen=True)
class ElabCtorInfo:
    name: str
    fields: list[tuple[str, Expr]]
    recursive_fields: list[ElabRecFieldInfo]
    target_index_exprs: list[Expr]


@dataclass
class InductiveSpec:
    name: str
    kind: str
    param_telescope: list[tuple[str, Expr]]
    index_telescope: list[tuple[str, Expr]]
    constructors: dict[str, ElabCtorInfo]
    eliminator_name: str
    projection_names: dict[str, str] = field(default_factory=dict)

    @property
    def param_count(self) -> int:
        return len(self.param_telescope)

    @property
    def index_count(self) -> int:
        return len(self.index_telescope)

    @property
    def constructor_names(self) -> list[str]:
        return list(self.constructors)


@dataclass
class ElabState:
    global_tags: dict[str, str] = field(default_factory=dict)
    inductives: dict[str, InductiveSpec] = field(default_factory=dict)
    example_counter: int = 0
    _fresh_counters: dict[str, int] = field(default_factory=dict)
    _surface_counter: int = 0

    # prefixes that appear at most once per telescope — never need a counter
    _SINGLETON = frozenset({"motive", "q", "d"})

    def fresh(self, prefix: str) -> str:
        stem = prefix or "x"
        if stem in self._SINGLETON:
            return f"_{stem}"
        c = self._fresh_counters.get(stem, 0)
        while True:
            name = f"_{stem}" if c == 0 else f"_{stem}{c}"
            c += 1
            if name not in self.global_tags:
                self._fresh_counters[stem] = c
                return name

    def fresh_example_name(self) -> str:
        while True:
            self.example_counter += 1
            name = f"example_{self.example_counter}"
            if name not in self.global_tags:
                return name

    def register_global(self, name: str, tag: str) -> None:
        self.global_tags[name] = tag


@dataclass(frozen=True)
class ElaborationResult:
    restricted_decls: list[RDecl]
    core_decls: list


def extend_local(local: ElabLocalContext, name: str) -> ElabLocalContext:
    return ElabLocalContext(local.names + [name])


class Elaborator:
    def __init__(self) -> None:
        self.state = ElabState()

    def allocate_surface(self) -> int:
        self.state._surface_counter += 1
        return 500_000 + self.state._surface_counter

    def atom(self, text: str) -> AtomExpr:
        return AtomExpr(self.allocate_surface(), text)

    def actual_binder_name(self, raw_name: Optional[str], prefix: str) -> str:
        if raw_name is not None:
            return raw_name
        return self.state.fresh(prefix)

    def is_defined(self, name: str, *, extra: Optional[set[str]] = None) -> bool:
        return name in self.state.global_tags or (extra is not None and name in extra)

    def resolve_dot(self, text: str) -> Optional[str]:
        if "." not in text:
            return text if text in self.state.global_tags else None
        fst, snd = text.split(".", 1)
        spec = self.state.inductives.get(fst)
        if spec is None:
            return text if text in self.state.global_tags else None
        if spec.kind == "inductive" and snd == "rec":
            return spec.eliminator_name
        if spec.kind in {"sum", "product"} and snd == "elim":
            return spec.eliminator_name
        if spec.kind == "product" and snd == "mk":
            return spec.constructor_names[0]
        if spec.kind == "product" and snd in spec.projection_names:
            return spec.projection_names[snd]
        return None

    def check_fresh_global(self, name: str, *, extra_reserved: Optional[set[str]] = None) -> None:
        if name in self.state.global_tags or (extra_reserved and name in extra_reserved):
            raise ElabError(f"duplicate global name: {name}")

    def elaborate_result(self, decls: list[Decl]) -> ElaborationResult:
        restricted_decls: list[RDecl] = []
        for decl in decls:
            restricted_decls.extend(self.elab_decl(decl))
        core_decls = [restricted_decl_to_core(decl) for decl in restricted_decls]
        return ElaborationResult(restricted_decls, core_decls)

    def elaborate(self, decls: list[Decl]) -> list:
        return self.elaborate_result(decls).core_decls

    def elaborate_restricted(self, decls: list[Decl]) -> list[RDecl]:
        return self.elaborate_result(decls).restricted_decls

    def elab_decl(self, decl: Decl) -> list[RDecl]:
        if isinstance(decl, VarDecl):
            core = self.elab_var_decl(decl)
            self.state.register_global(core.name, "global")
            return [core]
        if isinstance(decl, FunDecl):
            core = self.elab_fun_decl(decl)
            self.state.register_global(core.name, "global")
            return [core]
        if isinstance(decl, InductiveDecl):
            return self.elab_inductive_decl(decl)
        if isinstance(decl, ProductDecl):
            return self.elab_product_decl(decl)
        if isinstance(decl, AxiomDecl):
            core = self.elab_axiom_decl(decl)
            self.state.register_global(core.name, "global")
            return [core]
        if isinstance(decl, ExampleDecl):
            core = self.elab_example_decl(decl)
            self.state.register_global(core.name, "global")
            return [core]
        if isinstance(decl, EquationDecl):
            core = self.elab_equation_decl(decl)
            self.state.register_global(core.name, "global")
            return [core]
        raise ElabError(f"unknown declaration: {type(decl).__name__}")

    def elab_var_decl(self, decl: VarDecl) -> RDefinition:
        self.check_fresh_global(decl.name)
        local = ElabLocalContext()
        typ = self.lower_expr(decl.typ, local)
        value = self.lower_expr(decl.value, local)
        return RDefinition(decl.name, typ, value, decl.kind)

    def elab_fun_decl(self, decl: FunDecl) -> RDefinition:
        self.check_fresh_global(decl.name)
        local = ElabLocalContext()
        telescope, extended = self.lower_telescope(decl.params, local, prefix="p")
        ret_type = self.lower_expr(decl.ret_type, extended)
        body = self.lower_expr(decl.body, extended)
        return RDefinition(
            decl.name,
            fold_pi_expr(telescope, ret_type, self.allocate_surface),
            fold_lam_expr(telescope, body, self.allocate_surface),
            decl.kind,
        )

    def elab_axiom_decl(self, decl: AxiomDecl) -> RDefinition:
        self.check_fresh_global(decl.name)
        return RDefinition(decl.name, self.lower_expr(decl.typ, ElabLocalContext()), None, "axiom")

    def elab_example_decl(self, decl: ExampleDecl) -> RDefinition:
        name = self.state.fresh_example_name()
        local = ElabLocalContext()
        return RDefinition(
            name,
            self.lower_expr(decl.typ, local),
            self.lower_expr(decl.value, local),
            "var",
        )

    def elab_equation_decl(self, decl: EquationDecl) -> RDefinition:
        self.check_fresh_global(decl.name)
        typ = self.lower_expr(decl.typ, ElabLocalContext())
        telescope, _ = split_pi_expr(typ)
        if len(decl.params) > len(telescope):
            raise ElabError("equation parameters exceed Pi heads")
        binders: list[tuple[str, Expr]] = []
        current_local = ElabLocalContext()
        for (_, binder_ty), raw_name in zip(telescope, decl.params):
            actual = self.actual_binder_name(raw_name, "eq")
            binders.append((actual, binder_ty))
            current_local = extend_local(current_local, actual)
        body = self.lower_expr(decl.value, current_local)
        return RDefinition(decl.name, typ, fold_lam_expr(binders, body, self.allocate_surface), "var")

    def elab_product_decl(self, decl: ProductDecl) -> list[RDecl]:
        seen: set[str] = set()
        for field in decl.fields:
            if field.name in {"mk", "elim"}:
                raise ElabError(f"product field name cannot be {field.name}")
            if field.name in seen:
                raise ElabError(f"duplicate product field: {field.name}")
            seen.add(field.name)

        lowered_fields: list[tuple[str, Expr]] = []
        local = ElabLocalContext()
        for field in decl.fields:
            lowered_type = self.lower_expr(field.typ, local)
            lowered_fields.append((field.name, lowered_type))
            local = extend_local(local, field.name)

        spec = InductiveSpec(
            name=decl.name,
            kind="product",
            param_telescope=[],
            index_telescope=[],
            constructors={
                f"{decl.name}.mk": ElabCtorInfo(
                    name=f"{decl.name}.mk",
                    fields=lowered_fields,
                    recursive_fields=[],
                    target_index_exprs=[],
                )
            },
            eliminator_name=f"{decl.name}.elim",
        )
        return self.materialize_family(spec, product_field_names=[field.name for field in decl.fields])

    def elab_inductive_decl(self, decl: InductiveDecl) -> list[RDecl]:
        if decl.kind == "sum" and decl.params:
            raise ElabError("sum declarations cannot have parameters")

        self.check_fresh_global(decl.name)
        ctor_names = [ctor.name for ctor in decl.ctors]
        reserved = {decl.name, *ctor_names}
        elim_name = f"{decl.name}.rec" if decl.kind == "inductive" else f"{decl.name}.elim"
        reserved.add(elim_name)
        for name in reserved:
            if name != decl.name:
                self.check_fresh_global(name, extra_reserved=reserved - {name})

        local = ElabLocalContext()
        param_telescope, param_local = self.lower_telescope(decl.params, local, prefix="p")
        arity = self.atom("Type") if decl.arity is None else self.lower_expr(decl.arity, param_local, extra={decl.name})
        index_telescope, end = split_pi_expr(arity)
        if not self.is_type_atom(end):
            raise ElabError(f"{decl.name} arity must end in Type")

        constructors: dict[str, ElabCtorInfo] = {}
        for ctor in decl.ctors:
            if ctor.name in constructors:
                raise ElabError(f"duplicate constructor: {ctor.name}")
            constructors[ctor.name] = self.lower_constructor_info(
                decl.kind,
                decl.name,
                param_telescope,
                index_telescope,
                ctor,
            )

        spec = InductiveSpec(
            name=decl.name,
            kind=decl.kind,
            param_telescope=param_telescope,
            index_telescope=index_telescope,
            constructors=constructors,
            eliminator_name=elim_name,
        )
        return self.materialize_family(spec)

    def materialize_family(
        self,
        spec: InductiveSpec,
        *,
        product_field_names: Optional[list[str]] = None,
    ) -> list[RDecl]:
        self.state.register_global(spec.name, "type_ctor")
        for ctor_name in spec.constructor_names:
            self.state.register_global(ctor_name, "data_ctor")
        self.state.register_global(spec.eliminator_name, "eliminator")

        self.state.inductives[spec.name] = spec

        type_decl = RTypeCtorDecl(
            spec.name,
            spec.param_telescope,
            spec.index_telescope,
            fold_pi_expr(
                spec.param_telescope + spec.index_telescope,
                self.atom("Type"),
                self.allocate_surface,
            ),
            "type_ctor",
            spec.kind,
            spec.constructor_names,
        )

        data_decls = [
            self.build_data_ctor_decl(spec, ctor)
            for ctor in spec.constructors.values()
        ]

        motive_type = self.build_motive_type(spec)
        elim_type = self.build_eliminator_type(spec, motive_type)
        elim_decl = REliminatorDecl(
            spec.eliminator_name,
            spec.name,
            motive_type,
            elim_type,
            "rec" if spec.kind == "inductive" else "elim",
            spec.constructor_names,
        )

        result: list[RDecl] = [type_decl, *data_decls, elim_decl]

        if spec.kind == "product":
            if product_field_names is None:
                raise ElabError("product projection generation requires field names")
            projections = self.build_product_projections(spec, product_field_names)
            result.extend(projections)

        return result

    def build_data_ctor_decl(self, spec: InductiveSpec, ctor: ElabCtorInfo) -> RDataCtorDecl:
        ctor_type = fold_pi_expr(
            spec.param_telescope + ctor.fields,
            mk_surface_apps(
                self.atom(spec.name),
                [self.atom(name) for name, _ in spec.param_telescope] + ctor.target_index_exprs,
                allocator=self.allocate_surface,
            ),
            self.allocate_surface,
        )
        return RDataCtorDecl(
            ctor.name,
            spec.name,
            ctor_type,
            "data_ctor",
            spec.param_telescope + ctor.fields,
            spec.param_count,
            ctor.target_index_exprs,
            [
                RRecFieldInfo(
                    rec.field_position,
                    rec.recursive_kind,
                    rec.ho_telescope,
                    rec.target_index_exprs,
                )
                for rec in ctor.recursive_fields
            ],
        )

    def build_motive_type(self, spec: InductiveSpec) -> Expr:
        q_name = self.state.fresh("q")
        q_type = mk_surface_apps(
            self.atom(spec.name),
            [self.atom(name) for name, _ in spec.param_telescope + spec.index_telescope],
            allocator=self.allocate_surface,
        )
        return fold_pi_expr(
            spec.param_telescope + spec.index_telescope + [(q_name, q_type)],
            self.atom("Type"),
            self.allocate_surface,
        )

    def build_eliminator_type(self, spec: InductiveSpec, motive_type: Expr) -> Expr:
        motive_name = self.state.fresh("motive")
        q_name = self.state.fresh("q")
        q_type = mk_surface_apps(
            self.atom(spec.name),
            [self.atom(name) for name, _ in spec.param_telescope + spec.index_telescope],
            allocator=self.allocate_surface,
        )
        result = mk_surface_apps(
            self.atom(motive_name),
            [self.atom(name) for name, _ in spec.param_telescope + spec.index_telescope] + [self.atom(q_name)],
            allocator=self.allocate_surface,
        )
        body = fold_pi_expr(
            spec.index_telescope + [(q_name, q_type)],
            result,
            self.allocate_surface,
        )
        branch_binders = [
            (
                f"branch_{ctor.name}",
                self.build_eliminator_branch_type(spec, ctor, motive_name),
            )
            for ctor in spec.constructors.values()
        ]
        return fold_pi_expr(
            spec.param_telescope + [(motive_name, motive_type)] + branch_binders,
            body,
            self.allocate_surface,
        )

    def build_eliminator_branch_type(self, spec: InductiveSpec, ctor: ElabCtorInfo, motive_name: str) -> Expr:
        ctor_expr = mk_surface_apps(
            self.atom(ctor.name),
            [self.atom(name) for name, _ in spec.param_telescope + ctor.fields],
            allocator=self.allocate_surface,
        )
        result = mk_surface_apps(
            self.atom(motive_name),
            [self.atom(name) for name, _ in spec.param_telescope] + ctor.target_index_exprs + [ctor_expr],
            allocator=self.allocate_surface,
        )
        if spec.kind == "inductive":
            ih_binders = [
                (
                    self.ih_default_name(ctor.fields[rec.field_position][0]),
                    self.build_original_ih_type(spec, ctor, rec, motive_name),
                )
                for rec in ctor.recursive_fields
            ]
            result = fold_pi_expr(ih_binders, result, self.allocate_surface)
        return fold_pi_expr(ctor.fields, result, self.allocate_surface)

    def build_original_ih_type(
        self,
        spec: InductiveSpec,
        ctor: ElabCtorInfo,
        rec: ElabRecFieldInfo,
        motive_name: str,
    ) -> Expr:
        field_name = ctor.fields[rec.field_position][0]
        param_atoms = [self.atom(name) for name, _ in spec.param_telescope]
        if rec.recursive_kind == "direct":
            return mk_surface_apps(
                self.atom(motive_name),
                param_atoms + rec.target_index_exprs + [self.atom(field_name)],
                allocator=self.allocate_surface,
            )
        ho_atoms = [self.atom(name) for name, _ in rec.ho_telescope]
        field_applied = mk_surface_apps(self.atom(field_name), ho_atoms, allocator=self.allocate_surface)
        body = mk_surface_apps(
            self.atom(motive_name),
            param_atoms + rec.target_index_exprs + [field_applied],
            allocator=self.allocate_surface,
        )
        return fold_pi_expr(rec.ho_telescope, body, self.allocate_surface)

    def build_product_projections(self, spec: InductiveSpec, product_field_names: list[str]) -> list[RDefinition]:
        if len(spec.constructor_names) != 1:
            raise ElabError("product must have exactly one constructor")
        ctor = spec.constructors[spec.constructor_names[0]]
        projections: list[RDefinition] = []
        for index, field_name in enumerate(product_field_names):
            proj_name = f"{spec.name}.{field_name}"
            self.check_fresh_global(proj_name)
            self.state.register_global(proj_name, "projection")
            spec.projection_names[field_name] = proj_name

            scrut_name = self.state.fresh("d")
            scrut_atom = self.atom(scrut_name)
            q_name = self.state.fresh("q")

            return_type = self.product_projection_result_type(spec, ctor, index, scrut_atom)
            motive_body = self.product_projection_result_type(spec, ctor, index, self.atom(q_name))
            motive_expr = LambdaExpr(
                self.allocate_surface(),
                Param(
                    self.allocate_surface(),
                    q_name,
                    self.atom(spec.name),
                ),
                motive_body,
            )

            branch_body = self.atom(ctor.fields[index][0])
            branch = fold_lam_expr(ctor.fields, branch_body, self.allocate_surface)
            value = LambdaExpr(
                self.allocate_surface(),
                Param(self.allocate_surface(), scrut_name, self.atom(spec.name)),
                mk_surface_apps(
                    self.atom(spec.eliminator_name),
                    [motive_expr, branch, scrut_atom],
                    allocator=self.allocate_surface,
                ),
            )
            typ = PiExpr(
                self.allocate_surface(),
                scrut_name,
                self.atom(spec.name),
                return_type,
            )
            projections.append(RDefinition(proj_name, typ, value, "projection"))
        return projections

    def product_projection_result_type(
        self,
        spec: InductiveSpec,
        ctor: ElabCtorInfo,
        field_index: int,
        scrutinee_expr: Expr,
    ) -> Expr:
        mapping: dict[str, Expr] = {}
        for prev_name, _ in ctor.fields[:field_index]:
            mapping[prev_name] = mk_surface_apps(
                self.atom(f"{spec.name}.{prev_name}"),
                [scrutinee_expr],
                allocator=self.allocate_surface,
            )
        field_type = ctor.fields[field_index][1]
        return substitute_expr(
            field_type,
            mapping,
            fresh_name=self.state.fresh,
            allocator=self.allocate_surface,
        )

    def lower_constructor_info(
        self,
        decl_kind: str,
        owner: str,
        param_telescope: list[tuple[str, Expr]],
        index_telescope: list[tuple[str, Expr]],
        ctor: CtorDecl,
    ) -> ElabCtorInfo:
        extra = {owner}
        lowered_user_type = self.lower_expr(
            ctor.typ,
            ElabLocalContext([name for name, _ in param_telescope]),
            extra=extra,
        )
        full_type = fold_pi_expr(param_telescope, lowered_user_type, self.allocate_surface)
        full_telescope, result = split_pi_expr(full_type)
        head, args = unfold_surface_app(result)
        if not isinstance(head, AtomExpr) or head.text != owner:
            raise ElabError(f"constructor {ctor.name} must return {owner}")
        if len(args) != len(param_telescope) + len(index_telescope):
            raise ElabError(f"constructor {ctor.name} has wrong family arity")
        for expected, actual in zip([name for name, _ in param_telescope], args[: len(param_telescope)]):
            if not isinstance(actual, AtomExpr) or actual.text != expected:
                raise ElabError(f"constructor {ctor.name} must use uniform parameters")

        fields = full_telescope[len(param_telescope) :]
        recursive_fields = self.compile_recursive_fields(
            decl_kind,
            owner,
            [name for name, _ in param_telescope],
            len(index_telescope),
            fields,
        )
        return ElabCtorInfo(
            ctor.name,
            fields,
            recursive_fields,
            args[len(param_telescope) :],
        )

    def compile_recursive_fields(
        self,
        decl_kind: str,
        owner: str,
        param_names: list[str],
        index_count: int,
        field_telescope: list[tuple[str, Expr]],
    ) -> list[ElabRecFieldInfo]:
        result: list[ElabRecFieldInfo] = []
        family_arity = len(param_names) + index_count
        for index, (_, field_type) in enumerate(field_telescope):
            if decl_kind in {"sum", "product"} and contains_atom(field_type, owner):
                raise ElabError(f"{decl_kind} constructor field cannot mention {owner}")
            ho_telescope, tail = split_pi_expr(field_type)
            head, args = unfold_surface_app(tail)
            if not isinstance(head, AtomExpr) or head.text != owner:
                continue
            if len(args) != family_arity:
                raise ElabError(f"recursive field of {owner} has wrong family arity")
            for expected, actual in zip(param_names, args[: len(param_names)]):
                if not isinstance(actual, AtomExpr) or actual.text != expected:
                    raise ElabError(f"recursive field of {owner} must use uniform parameters")
            result.append(
                ElabRecFieldInfo(
                    index,
                    "higher" if ho_telescope else "direct",
                    ho_telescope,
                    args[len(param_names) :],
                )
            )
        return result

    def lower_telescope(
        self,
        params: list[Param],
        local: ElabLocalContext,
        *,
        prefix: str,
    ) -> tuple[list[tuple[str, Expr]], ElabLocalContext]:
        telescope: list[tuple[str, Expr]] = []
        current = local
        for param in params:
            actual = self.actual_binder_name(param.name, prefix)
            typ = self.lower_expr(param.typ, current)
            telescope.append((actual, typ))
            current = extend_local(current, actual)
        return telescope, current

    def lower_expr(
        self,
        expr: Expr,
        local: ElabLocalContext,
        *,
        extra: Optional[set[str]] = None,
    ) -> Expr:
        if isinstance(expr, AtomExpr):
            if expr.text in {"Type", "Prop"}:
                return self.atom("Type")
            if expr.text in local.names:
                return self.atom(expr.text)
            resolved = self.resolve_dot(expr.text) if "." in expr.text else None
            if resolved is not None:
                return self.atom(resolved)
            if self.is_defined(expr.text, extra=extra):
                return self.atom(expr.text)
            raise ElabError(f"unbound name: {expr.text}")
        if isinstance(expr, AppExpr):
            return AppExpr(
                self.allocate_surface(),
                self.lower_expr(expr.func, local, extra=extra),
                self.lower_expr(expr.arg, local, extra=extra),
            )
        if isinstance(expr, ArrowExpr):
            name = self.actual_binder_name(None, "a")
            domain = self.lower_expr(expr.domain, local, extra=extra)
            codomain = self.lower_expr(expr.codomain, extend_local(local, name), extra=extra)
            return PiExpr(self.allocate_surface(), name, domain, codomain)
        if isinstance(expr, PiExpr):
            name = self.actual_binder_name(expr.name, "pi")
            domain = self.lower_expr(expr.domain, local, extra=extra)
            codomain = self.lower_expr(expr.codomain, extend_local(local, name), extra=extra)
            return PiExpr(self.allocate_surface(), name, domain, codomain)
        if isinstance(expr, LambdaExpr):
            name = self.actual_binder_name(expr.param.name, "lam")
            param_type = self.lower_expr(expr.param.typ, local, extra=extra)
            body = self.lower_expr(expr.body, extend_local(local, name), extra=extra)
            return LambdaExpr(
                self.allocate_surface(),
                Param(self.allocate_surface(), name, param_type),
                body,
            )
        if isinstance(expr, LetExpr):
            name = self.actual_binder_name(expr.name, "let")
            typ = self.lower_expr(expr.typ, local, extra=extra)
            value = self.lower_expr(expr.value, local, extra=extra)
            body = self.lower_expr(expr.body, extend_local(local, name), extra=extra)
            return AppExpr(
                self.allocate_surface(),
                LambdaExpr(
                    self.allocate_surface(),
                    Param(self.allocate_surface(), name, typ),
                    body,
                ),
                value,
            )
        if isinstance(expr, EqExpr):
            return mk_surface_apps(
                self.atom("Eq"),
                [
                    self.lower_expr(expr.typ, local, extra=extra),
                    self.lower_expr(expr.lhs, local, extra=extra),
                    self.lower_expr(expr.rhs, local, extra=extra),
                ],
                allocator=self.allocate_surface,
            )
        if isinstance(expr, ProductExpr):
            spec = self.state.inductives.get(expr.type_name)
            if spec is None or spec.kind != "product":
                raise ElabError(f"{expr.type_name} is not a product type")
            return mk_surface_apps(
                self.atom(spec.constructor_names[0]),
                [self.lower_expr(arg, local, extra=extra) for arg in expr.args],
                allocator=self.allocate_surface,
            )
        if isinstance(expr, MatchExpr):
            return self.lower_match_like(expr, local, extra=extra, is_case=False)
        if isinstance(expr, CaseExpr):
            return self.lower_match_like(expr, local, extra=extra, is_case=True)
        raise ElabError(f"unsupported expression: {type(expr).__name__}")

    def lower_match_like(
        self,
        expr: MatchExpr | CaseExpr,
        local: ElabLocalContext,
        *,
        extra: Optional[set[str]],
        is_case: bool,
    ) -> Expr:
        family_name = expr.sum_type if isinstance(expr, CaseExpr) else expr.inductive
        spec = self.state.inductives.get(family_name)
        if spec is None:
            raise ElabError(f"unknown family in match/case: {family_name}")
        if is_case and spec.kind not in {"sum", "product"}:
            raise ElabError("case can only eliminate sum/product types")
        if not is_case and spec.kind != "inductive":
            raise ElabError("match can only eliminate inductive types")

        family_args_raw = expr.type_args if isinstance(expr, CaseExpr) else expr.family_args
        if len(family_args_raw) != spec.param_count + spec.index_count:
            raise ElabError("wrong number of family arguments in match/case")
        family_args = [self.lower_expr(arg, local, extra=extra) for arg in family_args_raw]
        scrutinee = self.lower_expr(expr.scrutinee, local, extra=extra)

        bind_raw = list(expr.bind_names)
        if bind_raw and len(bind_raw) != spec.param_count + spec.index_count:
            raise ElabError("bind count must match family arity or be omitted")
        if not bind_raw:
            bind_raw = [None] * (spec.param_count + spec.index_count)
        bind_actuals = [self.actual_binder_name(name, "b") for name in bind_raw]
        param_actuals = bind_actuals[: spec.param_count]
        index_actuals = bind_actuals[spec.param_count :]

        motive_local = ElabLocalContext(local.names + bind_actuals)
        alias_actual = self.actual_binder_name(expr.alias, "q")
        motive_local = extend_local(motive_local, alias_actual)
        motive_body = self.lower_expr(expr.motive_body, motive_local, extra=extra)

        param_bindings = self.instantiate_telescope(spec.param_telescope, param_actuals)
        index_bindings = self.instantiate_index_telescope(spec, param_actuals, index_actuals)
        alias_type = mk_surface_apps(
            self.atom(spec.name),
            [self.atom(name) for name in bind_actuals],
            allocator=self.allocate_surface,
        )
        motive_expr = fold_lam_expr(
            param_bindings + index_bindings + [(alias_actual, alias_type)],
            motive_body,
            self.allocate_surface,
        )

        branches = expr.branches
        if len(branches) != len(spec.constructor_names):
            raise ElabError("branches must be complete and ordered")
        branch_exprs: list[Expr] = []
        for branch, ctor_name in zip(branches, spec.constructor_names):
            if branch.ctor != ctor_name:
                raise ElabError(f"expected branch {ctor_name}, got {branch.ctor}")
            ctor = spec.constructors[ctor_name]
            branch_exprs.append(
                self.build_match_branch(
                    spec,
                    ctor,
                    branch,
                    local,
                    extra,
                    family_args[: spec.param_count],
                    {
                        actual: term
                        for actual, term in zip(param_actuals, family_args[: spec.param_count])
                    },
                    index_actuals,
                    motive_expr,
                    allow_ih=not is_case,
                )
            )

        return mk_surface_apps(
            self.atom(spec.eliminator_name),
            family_args[: spec.param_count]
            + [motive_expr]
            + branch_exprs
            + family_args[spec.param_count :]
            + [scrutinee],
            allocator=self.allocate_surface,
        )

    def instantiate_telescope(
        self,
        telescope: list[tuple[str, Expr]],
        actuals: list[str],
    ) -> list[tuple[str, Expr]]:
        mapping: dict[str, Expr] = {}
        result: list[tuple[str, Expr]] = []
        for (orig_name, orig_type), actual in zip(telescope, actuals):
            typ = substitute_expr(
                orig_type,
                mapping,
                fresh_name=self.state.fresh,
                allocator=self.allocate_surface,
            )
            result.append((actual, typ))
            mapping[orig_name] = self.atom(actual)
        return result

    def instantiate_index_telescope(
        self,
        spec: InductiveSpec,
        param_actuals: list[str],
        index_actuals: list[str],
    ) -> list[tuple[str, Expr]]:
        mapping = {
            orig_name: self.atom(actual)
            for (orig_name, _), actual in zip(spec.param_telescope, param_actuals)
        }
        result: list[tuple[str, Expr]] = []
        for (orig_name, orig_type), actual in zip(spec.index_telescope, index_actuals):
            typ = substitute_expr(
                orig_type,
                mapping,
                fresh_name=self.state.fresh,
                allocator=self.allocate_surface,
            )
            result.append((actual, typ))
            mapping[orig_name] = self.atom(actual)
        return result

    def build_match_branch(
        self,
        spec: InductiveSpec,
        ctor: ElabCtorInfo,
        branch: MatchBranch | CaseBranch,
        outer_local: ElabLocalContext,
        extra: Optional[set[str]],
        param_terms: list[Expr],
        param_alias_terms: dict[str, Expr],
        index_actuals: list[str],
        motive_expr: Expr,
        *,
        allow_ih: bool,
    ) -> Expr:
        del index_actuals
        field_count = len(ctor.fields)
        rec_count = len(ctor.recursive_fields)
        if len(branch.fields) != field_count:
            raise ElabError(f"branch {ctor.name} expected {field_count} fields")
        if allow_ih:
            ih_raw = list(branch.ihs)
            if ih_raw and len(ih_raw) != rec_count:
                raise ElabError(f"branch {ctor.name} expected {rec_count} IH names")
            if not ih_raw:
                ih_raw = [None] * rec_count
        else:
            if isinstance(branch, MatchBranch) and branch.ihs:
                raise ElabError("case branches cannot bind IHs")
            ih_raw = []

        param_mapping = {
            orig_name: actual
            for (orig_name, _), actual in zip(spec.param_telescope, param_terms)
        }

        field_actuals: list[str] = []
        field_types: list[tuple[str, Expr]] = []
        field_mapping = dict(param_mapping)
        for (orig_name, orig_type), raw_name in zip(ctor.fields, branch.fields):
            actual = self.actual_binder_name(raw_name, "f")
            actual_type = substitute_expr(
                orig_type,
                field_mapping,
                fresh_name=self.state.fresh,
                allocator=self.allocate_surface,
            )
            field_actuals.append(actual)
            field_types.append((actual, actual_type))
            field_mapping[orig_name] = self.atom(actual)

        ih_bindings: list[tuple[str, Expr]] = []
        for rec, raw_ih in zip(ctor.recursive_fields, ih_raw):
            actual = self.actual_binder_name(raw_ih, "ih")
            ih_type = self.build_instantiated_ih_type(
                spec,
                ctor,
                rec,
                param_terms,
                field_actuals,
                motive_expr,
            )
            ih_bindings.append((actual, ih_type))

        branch_local = ElabLocalContext(
            outer_local.names
            + list(param_alias_terms.keys())
            + field_actuals
            + [name for name, _ in ih_bindings]
        )
        lowered_body = self.lower_expr(branch.body, branch_local, extra=extra)
        lowered_body = substitute_expr(
            lowered_body,
            param_alias_terms,
            fresh_name=self.state.fresh,
            allocator=self.allocate_surface,
        )
        return fold_lam_expr(
            field_types + ih_bindings,
            lowered_body,
            self.allocate_surface,
        )

    def build_instantiated_ih_type(
        self,
        spec: InductiveSpec,
        ctor: ElabCtorInfo,
        rec: ElabRecFieldInfo,
        param_terms: list[Expr],
        field_actuals: list[str],
        motive_expr: Expr,
    ) -> Expr:
        mapping = {
            orig_name: actual
            for (orig_name, _), actual in zip(spec.param_telescope, param_terms)
        }
        for (orig_name, _), actual in zip(ctor.fields[: rec.field_position], field_actuals[: rec.field_position]):
            mapping[orig_name] = self.atom(actual)
        field_actual = field_actuals[rec.field_position]

        if rec.recursive_kind == "direct":
            target_indices = [
                substitute_expr(
                    arg,
                    mapping,
                    fresh_name=self.state.fresh,
                    allocator=self.allocate_surface,
                )
                for arg in rec.target_index_exprs
            ]
            return mk_surface_apps(
                motive_expr,
                list(param_terms) + target_indices + [self.atom(field_actual)],
                allocator=self.allocate_surface,
            )

        ho_bindings: list[tuple[str, Expr]] = []
        ho_mapping = dict(mapping)
        ho_actuals: list[str] = []
        for orig_name, orig_type in rec.ho_telescope:
            actual = self.actual_binder_name(orig_name, "h")
            actual_type = substitute_expr(
                orig_type,
                ho_mapping,
                fresh_name=self.state.fresh,
                allocator=self.allocate_surface,
            )
            ho_bindings.append((actual, actual_type))
            ho_mapping[orig_name] = self.atom(actual)
            ho_actuals.append(actual)

        target_indices = [
            substitute_expr(
                arg,
                ho_mapping,
                fresh_name=self.state.fresh,
                allocator=self.allocate_surface,
            )
            for arg in rec.target_index_exprs
        ]
        field_applied = mk_surface_apps(
            self.atom(field_actual),
            [self.atom(name) for name in ho_actuals],
            allocator=self.allocate_surface,
        )
        body = mk_surface_apps(
            motive_expr,
            list(param_terms) + target_indices + [field_applied],
            allocator=self.allocate_surface,
        )
        return fold_pi_expr(ho_bindings, body, self.allocate_surface)

    def ih_default_name(self, field_name: str) -> str:
        return f"ih_{field_name}"

    def is_type_atom(self, expr: Expr) -> bool:
        return isinstance(expr, AtomExpr) and expr.text in {"Type", "Prop"}


def elaborate_result(decls: list[Decl]) -> ElaborationResult:
    return Elaborator().elaborate_result(decls)


def elaborate_restricted(decls: list[Decl]) -> list[RDecl]:
    return Elaborator().elaborate_restricted(decls)


def elaborate(decls: list[Decl]) -> list:
    return Elaborator().elaborate(decls)
