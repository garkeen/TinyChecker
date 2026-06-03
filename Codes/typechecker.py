from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .core import (
    CApp,
    CDataCtorDecl,
    CDecl,
    CDefinition,
    CEliminatorDecl,
    CGlobal,
    CLam,
    CPi,
    CTerm,
    CType,
    CTypeCtorDecl,
    CVar,
)
from .core_ops import instantiate, shift
from .errors import TypeCheckError
from .pretty import show_term
from .reducer import ConvStrategy, Reducer
from .runtime import ConstructorInfo, EliminatorInfo, GlobalEntry, InductiveInfo, TcGlobalContext


@dataclass
class TcContext:
    types: list[CTerm] = field(default_factory=list)
    names: list[Optional[str]] = field(default_factory=list)


def extend_context(ctx: TcContext, name: Optional[str], ty: CTerm) -> TcContext:
    return TcContext([ty] + ctx.types, [name] + ctx.names)


class TypeChecker:
    def __init__(self, *, conv_strategy: ConvStrategy = "whnf") -> None:
        self.global_ctx = TcGlobalContext()
        self.conv_strategy = conv_strategy

    def check_program(self, decls: list[CDecl]) -> TcGlobalContext:
        for decl in decls:
            self.check_decl(decl)
        return self.global_ctx

    def register_global(self, entry: GlobalEntry) -> None:
        if entry.name in self.global_ctx.globals:
            raise TypeCheckError(f"duplicate global name: {entry.name}")
        self.global_ctx.globals[entry.name] = entry

    def check_decl(self, decl: CDecl) -> None:
        if isinstance(decl, CDefinition):
            self.check_definition(decl)
            return
        if isinstance(decl, CTypeCtorDecl):
            self.check_type_ctor(decl)
            return
        if isinstance(decl, CDataCtorDecl):
            self.check_data_ctor(decl)
            return
        if isinstance(decl, CEliminatorDecl):
            self.check_eliminator(decl)
            return
        raise TypeCheckError(f"unknown core declaration: {type(decl).__name__}")

    def check_definition(self, decl: CDefinition) -> None:
        ctx = TcContext()
        self.check(decl.typ, CType(), ctx)
        if decl.value is not None:
            self.check(decl.value, decl.typ, ctx)
        self.register_global(GlobalEntry(decl.name, decl.typ, decl.value, decl.kind))

    def check_type_ctor(self, decl: CTypeCtorDecl) -> None:
        ctx = TcContext()
        self.check(decl.typ, CType(), ctx)
        elim_name = f"{decl.name}.rec" if decl.family_kind == "inductive" else f"{decl.name}.elim"
        self.global_ctx.inductives[decl.name] = InductiveInfo(
            name=decl.name,
            kind=decl.family_kind,
            param_count=len(decl.param_telescope),
            index_count=len(decl.index_telescope),
            index_telescope=decl.index_telescope,
            constructor_names=decl.constructor_names,
            eliminator_name=elim_name,
        )
        self.register_global(GlobalEntry(decl.name, decl.typ, None, "type_ctor"))

    def check_data_ctor(self, decl: CDataCtorDecl) -> None:
        ctx = TcContext()
        self.check(decl.typ, CType(), ctx)
        ind = self.global_ctx.inductives.get(decl.owner)
        if ind is None:
            raise TypeCheckError(f"unknown owner inductive: {decl.owner}")
        self.global_ctx.constructors[decl.name] = ConstructorInfo(
            name=decl.name,
            owner=decl.owner,
            typ=decl.typ,
            param_count=ind.param_count,
            fields=decl.constructor_parameter_list,
            recursive_fields=decl.recursive_fields,
            target_args=decl.target_args,
        )
        self.register_global(GlobalEntry(decl.name, decl.typ, None, "data_ctor"))

    def check_eliminator(self, decl: CEliminatorDecl) -> None:
        self.global_ctx.eliminators[decl.name] = EliminatorInfo(
            name=decl.name,
            owner=decl.owner,
            typ=decl.typ,
            branch_order=decl.branch_order,
            kind=decl.kind,
        )
        ind = self.global_ctx.inductives[decl.owner]
        ind.eliminator_name = decl.name
        self.register_global(GlobalEntry(decl.name, decl.typ, None, decl.kind))

    def reducer(self) -> Reducer:
        return Reducer(self.global_ctx, conv_strategy=self.conv_strategy)

    def lookup_var(self, ctx: TcContext, index: int) -> CTerm:
        if index < 0 or index >= len(ctx.types):
            raise TypeCheckError(f"unbound de Bruijn variable: {index}")
        return shift(ctx.types[index], index + 1)

    def infer(self, term: CTerm, ctx: TcContext) -> CTerm:
        if isinstance(term, CType):
            return CType()
        if isinstance(term, CVar):
            return self.lookup_var(ctx, term.index)
        if isinstance(term, CGlobal):
            entry = self.global_ctx.globals.get(term.name)
            if entry is None:
                raise TypeCheckError(f"unbound global variable: {term.name}")
            return entry.typ
        if isinstance(term, CPi):
            self.check(term.domain, CType(), ctx)
            self.check(term.codomain, CType(), extend_context(ctx, term.name, term.domain))
            return CType()
        if isinstance(term, CLam):
            self.check(term.param_type, CType(), ctx)
            body_ty = self.infer(term.body, extend_context(ctx, term.name, term.param_type))
            return CPi(term.name, term.param_type, body_ty)
        if isinstance(term, CApp):
            func_ty = self.infer(term.func, ctx)
            func_ty = self.reducer().whnf(func_ty)
            if not isinstance(func_ty, CPi):
                raise TypeCheckError(f"non-function application: {show_term(func_ty)}")
            self.check(term.arg, func_ty.domain, ctx)
            return instantiate(func_ty.codomain, term.arg)
        raise TypeCheckError(f"cannot infer type of {term}")

    def check(self, term: CTerm, expected: CTerm, ctx: TcContext) -> None:
        expected_whnf = self.reducer().whnf(expected)
        if isinstance(term, CLam):
            if not isinstance(expected_whnf, CPi):
                raise TypeCheckError(f"lambda expected Pi type, got {show_term(expected)}")
            self.check(term.param_type, CType(), ctx)
            self.convert(term.param_type, expected_whnf.domain)
            self.check(
                term.body,
                expected_whnf.codomain,
                extend_context(ctx, term.name, expected_whnf.domain),
            )
            return
        inferred = self.infer(term, ctx)
        self.convert(inferred, expected)

    def convert(self, lhs: CTerm, rhs: CTerm) -> None:
        reducer = self.reducer()
        if not reducer.is_def_eq(lhs, rhs):
            raise TypeCheckError(
                f"type mismatch: {show_term(reducer.whnf(lhs))} vs {show_term(reducer.whnf(rhs))}"
            )


def check_program(decls: list[CDecl], *, conv_strategy: ConvStrategy = "whnf") -> TcGlobalContext:
    return TypeChecker(conv_strategy=conv_strategy).check_program(decls)
