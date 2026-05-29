from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional


@dataclass(frozen=True)
class SurfaceNode:
    surface_id: int = field(compare=False)


@dataclass(frozen=True)
class Expr(SurfaceNode):
    pass


@dataclass(frozen=True)
class Decl(SurfaceNode):
    pass


@dataclass(frozen=True)
class Param(SurfaceNode):
    name: Optional[str]
    typ: Expr


@dataclass(frozen=True)
class CtorDecl(SurfaceNode):
    name: str
    typ: Expr


@dataclass(frozen=True)
class FieldDecl(SurfaceNode):
    name: str
    typ: Expr


@dataclass(frozen=True)
class MatchBranch(SurfaceNode):
    ctor: str
    fields: list[Optional[str]]
    ihs: list[Optional[str]]
    body: Expr


@dataclass(frozen=True)
class CaseBranch(SurfaceNode):
    ctor: str
    fields: list[Optional[str]]
    body: Expr


@dataclass(frozen=True)
class VarDecl(Decl):
    kind: str
    name: str
    typ: Expr
    value: Expr


@dataclass(frozen=True)
class FunDecl(Decl):
    kind: str
    name: str
    params: list[Param]
    ret_type: Expr
    body: Expr


@dataclass(frozen=True)
class InductiveDecl(Decl):
    kind: str
    name: str
    params: list[Param]
    arity: Optional[Expr]
    ctors: list[CtorDecl]


@dataclass(frozen=True)
class ProductDecl(Decl):
    name: str
    fields: list[FieldDecl]


@dataclass(frozen=True)
class AxiomDecl(Decl):
    name: str
    typ: Expr


@dataclass(frozen=True)
class ExampleDecl(Decl):
    typ: Expr
    value: Expr


@dataclass(frozen=True)
class EquationDecl(Decl):
    name: str
    typ: Expr
    params: list[Optional[str]]
    value: Expr


@dataclass(frozen=True)
class AtomExpr(Expr):
    text: str


@dataclass(frozen=True)
class ArrowExpr(Expr):
    domain: Expr
    codomain: Expr


@dataclass(frozen=True)
class PiExpr(Expr):
    name: Optional[str]
    domain: Expr
    codomain: Expr


@dataclass(frozen=True)
class LambdaExpr(Expr):
    param: Param
    body: Expr


@dataclass(frozen=True)
class AppExpr(Expr):
    func: Expr
    arg: Expr


@dataclass(frozen=True)
class LetExpr(Expr):
    name: Optional[str]
    typ: Expr
    value: Expr
    body: Expr


@dataclass(frozen=True)
class MatchExpr(Expr):
    scrutinee: Expr
    alias: Optional[str]
    inductive: str
    family_args: list[Expr]
    bind_names: list[Optional[str]]
    motive_body: Expr
    branches: list[MatchBranch]


@dataclass(frozen=True)
class CaseExpr(Expr):
    scrutinee: Expr
    alias: Optional[str]
    sum_type: str
    type_args: list[Expr]
    bind_names: list[Optional[str]]
    motive_body: Expr
    branches: list[CaseBranch]


@dataclass(frozen=True)
class ProductExpr(Expr):
    type_name: str
    args: list[Expr]


@dataclass(frozen=True)
class EqExpr(Expr):
    typ: Expr
    lhs: Expr
    rhs: Expr


def unfold_surface_app(expr: Expr) -> tuple[Expr, list[Expr]]:
    args: list[Expr] = []
    current = expr
    while isinstance(current, AppExpr):
        args.append(current.arg)
        current = current.func
    args.reverse()
    return current, args


def mk_surface_apps(head: Expr, args: list[Expr], *, allocator) -> Expr:
    term = head
    for arg in args:
        term = AppExpr(surface_id=allocator(), func=term, arg=arg)
    return term
