from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Optional

from .core import CApp, CDecl, CDefinition, CEliminatorDecl, CGlobal, CLam, CPi, CRecursiveFieldDecl, CTerm, CType, CTypeCtorDecl, CDataCtorDecl, CVar
from .surface import AppExpr, AtomExpr, Expr, LambdaExpr, Param, PiExpr, mk_surface_apps, unfold_surface_app


@dataclass(frozen=True)
class RDefinition:
    name: str
    typ: Expr
    value: Optional[Expr]
    kind: str


@dataclass(frozen=True)
class RTypeCtorDecl:
    name: str
    param_telescope: list[tuple[str, Expr]]
    index_telescope: list[tuple[str, Expr]]
    typ: Expr
    kind: str
    family_kind: str
    constructor_names: list[str]


@dataclass(frozen=True)
class RRecFieldInfo:
    field_position: int
    recursive_kind: str
    ho_telescope: list[tuple[str, Expr]]
    target_index_exprs: list[Expr]


@dataclass(frozen=True)
class RDataCtorDecl:
    name: str
    owner: str
    typ: Expr
    kind: str
    constructor_parameter_list: list[tuple[str, Expr]]
    param_count: int
    target_index_exprs: list[Expr]
    recursive_fields: list[RRecFieldInfo]


@dataclass(frozen=True)
class REliminatorDecl:
    name: str
    owner: str
    motive_type: Expr
    typ: Expr
    kind: str
    branch_order: list[str]


RDecl = RDefinition | RTypeCtorDecl | RDataCtorDecl | REliminatorDecl


def lookup_local(local: list[str], name: str) -> Optional[int]:
    for i in range(len(local) - 1, -1, -1):
        if local[i] == name:
            return len(local) - 1 - i
    return None


def fold_pi_expr(telescope: list[tuple[str, Expr]], body: Expr, allocator: Callable[[], int]) -> Expr:
    result = body
    for name, typ in reversed(telescope):
        result = PiExpr(allocator(), name, typ, result)
    return result


def fold_lam_expr(telescope: list[tuple[str, Expr]], body: Expr, allocator: Callable[[], int]) -> Expr:
    result = body
    for name, typ in reversed(telescope):
        result = LambdaExpr(allocator(), Param(allocator(), name, typ), result)
    return result


def split_pi_expr(expr: Expr) -> tuple[list[tuple[str, Expr]], Expr]:
    telescope: list[tuple[str, Expr]] = []
    current = expr
    while isinstance(current, PiExpr):
        if current.name is None:
            raise ValueError("restricted Pi binder must be named")
        telescope.append((current.name, current.domain))
        current = current.codomain
    return telescope, current


def contains_atom(expr: Expr, text: str) -> bool:
    if isinstance(expr, AtomExpr):
        return expr.text == text
    if isinstance(expr, AppExpr):
        return contains_atom(expr.func, text) or contains_atom(expr.arg, text)
    if isinstance(expr, PiExpr):
        return contains_atom(expr.domain, text) or contains_atom(expr.codomain, text)
    if isinstance(expr, LambdaExpr):
        return contains_atom(expr.param.typ, text) or contains_atom(expr.body, text)
    return False


def free_vars(expr: Expr) -> set[str]:
    if isinstance(expr, AtomExpr):
        if expr.text in {"Type", "Prop"}:
            return set()
        return {expr.text}
    if isinstance(expr, AppExpr):
        return free_vars(expr.func) | free_vars(expr.arg)
    if isinstance(expr, PiExpr):
        if expr.name is None:
            raise ValueError("restricted Pi binder must be named")
        result = free_vars(expr.domain) | free_vars(expr.codomain)
        result.discard(expr.name)
        return result
    if isinstance(expr, LambdaExpr):
        if expr.param.name is None:
            raise ValueError("restricted lambda binder must be named")
        result = free_vars(expr.param.typ) | free_vars(expr.body)
        result.discard(expr.param.name)
        return result
    raise TypeError(f"unsupported restricted expression: {type(expr).__name__}")


def rename_bound_occurrences(expr: Expr, old: str, new: str, allocator: Callable[[], int]) -> Expr:
    if isinstance(expr, AtomExpr):
        return AtomExpr(expr.surface_id, new if expr.text == old else expr.text)
    if isinstance(expr, AppExpr):
        return AppExpr(
            expr.surface_id,
            rename_bound_occurrences(expr.func, old, new, allocator),
            rename_bound_occurrences(expr.arg, old, new, allocator),
        )
    if isinstance(expr, PiExpr):
        if expr.name is None:
            raise ValueError("restricted Pi binder must be named")
        domain = rename_bound_occurrences(expr.domain, old, new, allocator)
        if expr.name == old:
            return PiExpr(expr.surface_id, expr.name, domain, expr.codomain)
        return PiExpr(
            expr.surface_id,
            expr.name,
            domain,
            rename_bound_occurrences(expr.codomain, old, new, allocator),
        )
    if isinstance(expr, LambdaExpr):
        if expr.param.name is None:
            raise ValueError("restricted lambda binder must be named")
        param_type = rename_bound_occurrences(expr.param.typ, old, new, allocator)
        if expr.param.name == old:
            return LambdaExpr(
                expr.surface_id,
                Param(expr.param.surface_id, expr.param.name, param_type),
                expr.body,
            )
        return LambdaExpr(
            expr.surface_id,
            Param(expr.param.surface_id, expr.param.name, param_type),
            rename_bound_occurrences(expr.body, old, new, allocator),
        )
    raise TypeError(f"unsupported restricted expression: {type(expr).__name__}")


def substitute_expr(
    expr: Expr,
    mapping: dict[str, Expr],
    *,
    fresh_name: Callable[[str], str],
    allocator: Callable[[], int],
) -> Expr:
    """Capture-avoiding substitution: replace free occurrences of names in *mapping*.

    For binders (Pi / Lambda) the bound name is removed from the mapping
    before recursing into the body.  If the bound name appears in the free
    variables of any value still in the mapping, it is alpha-renamed to a
    fresh name to avoid capture.
    """
    if not mapping:
        return expr
    if isinstance(expr, AtomExpr):
        return mapping.get(expr.text, expr)
    if isinstance(expr, AppExpr):
        return AppExpr(
            expr.surface_id,
            substitute_expr(expr.func, mapping, fresh_name=fresh_name, allocator=allocator),
            substitute_expr(expr.arg, mapping, fresh_name=fresh_name, allocator=allocator),
        )
    if isinstance(expr, PiExpr):
        if expr.name is None:
            raise ValueError("restricted Pi binder must be named")
        domain = substitute_expr(expr.domain, mapping, fresh_name=fresh_name, allocator=allocator)
        body_mapping = {key: value for key, value in mapping.items() if key != expr.name}
        binder = expr.name
        forbidden = set().union(*(free_vars(value) for value in body_mapping.values())) if body_mapping else set()
        codomain = expr.codomain
        if binder in forbidden:
            fresh = fresh_name(binder or "x")
            codomain = rename_bound_occurrences(codomain, binder, fresh, allocator)
            binder = fresh
        codomain = substitute_expr(codomain, body_mapping, fresh_name=fresh_name, allocator=allocator)
        return PiExpr(expr.surface_id, binder, domain, codomain)
    if isinstance(expr, LambdaExpr):
        if expr.param.name is None:
            raise ValueError("restricted lambda binder must be named")
        param_type = substitute_expr(expr.param.typ, mapping, fresh_name=fresh_name, allocator=allocator)
        body_mapping = {key: value for key, value in mapping.items() if key != expr.param.name}
        binder = expr.param.name
        forbidden = set().union(*(free_vars(value) for value in body_mapping.values())) if body_mapping else set()
        body = expr.body
        if binder in forbidden:
            fresh = fresh_name(binder or "x")
            body = rename_bound_occurrences(body, binder, fresh, allocator)
            binder = fresh
        body = substitute_expr(body, body_mapping, fresh_name=fresh_name, allocator=allocator)
        return LambdaExpr(expr.surface_id, Param(expr.param.surface_id, binder, param_type), body)
    raise TypeError(f"unsupported restricted expression: {type(expr).__name__}")


def restricted_expr_to_core(expr: Expr, local: list[str]) -> CTerm:
    if isinstance(expr, AtomExpr):
        if expr.text in {"Type", "Prop"}:
            return CType()
        index = lookup_local(local, expr.text)
        if index is not None:
            return CVar(index, expr.text)
        return CGlobal(expr.text)
    if isinstance(expr, AppExpr):
        return CApp(
            restricted_expr_to_core(expr.func, local),
            restricted_expr_to_core(expr.arg, local),
        )
    if isinstance(expr, PiExpr):
        if expr.name is None:
            raise ValueError("restricted Pi binder must be named")
        domain = restricted_expr_to_core(expr.domain, local)
        codomain = restricted_expr_to_core(expr.codomain, local + [expr.name])
        return CPi(expr.name, domain, codomain)
    if isinstance(expr, LambdaExpr):
        if expr.param.name is None:
            raise ValueError("restricted lambda binder must be named")
        param_type = restricted_expr_to_core(expr.param.typ, local)
        body = restricted_expr_to_core(expr.body, local + [expr.param.name])
        return CLam(expr.param.name, param_type, body)
    raise TypeError(f"unsupported restricted expression: {type(expr).__name__}")


def restricted_decl_to_core(decl: RDecl) -> CDecl:
    if isinstance(decl, RDefinition):
        return CDefinition(
            decl.name,
            restricted_expr_to_core(decl.typ, []),
            None if decl.value is None else restricted_expr_to_core(decl.value, []),
            decl.kind,
        )
    if isinstance(decl, RTypeCtorDecl):
        return CTypeCtorDecl(
            decl.name,
            [
                (
                    name,
                    restricted_expr_to_core(typ, [n for n, _ in decl.param_telescope[:i]]),
                )
                for i, (name, typ) in enumerate(decl.param_telescope)
            ],
            [
                (
                    name,
                    restricted_expr_to_core(
                        typ,
                        [n for n, _ in decl.param_telescope] + [n for n, _ in decl.index_telescope[:i]],
                    ),
                )
                for i, (name, typ) in enumerate(decl.index_telescope)
            ],
            restricted_expr_to_core(decl.typ, []),
            decl.kind,
            decl.family_kind,
            decl.constructor_names,
        )
    if isinstance(decl, RDataCtorDecl):
        ctor_env_names = [name for name, _ in decl.constructor_parameter_list]
        result_head, result_args = unfold_surface_app(_final_result_of_ctor_type(decl.typ))
        if not isinstance(result_head, AtomExpr):
            raise ValueError("constructor result must be an atom-headed application")
        return CDataCtorDecl(
            decl.name,
            decl.owner,
            restricted_expr_to_core(decl.typ, []),
            decl.kind,
            [
                (
                    name,
                    restricted_expr_to_core(typ, ctor_env_names[:i]),
                )
                for i, (name, typ) in enumerate(decl.constructor_parameter_list)
            ],
            [
                restricted_expr_to_core(arg, ctor_env_names)
                for arg in [AtomExpr(-1, n) for n in ctor_env_names[: decl.param_count]] + decl.target_index_exprs
            ],
            [
                CRecursiveFieldDecl(
                    rec.field_position,
                    [
                        (
                            name,
                            restricted_expr_to_core(
                                typ,
                                ctor_env_names[: decl.param_count + rec.field_position] + [n for n, _ in rec.ho_telescope[:i]],
                            ),
                        )
                        for i, (name, typ) in enumerate(rec.ho_telescope)
                    ],
                    [
                        restricted_expr_to_core(arg, ctor_env_names[: decl.param_count + rec.field_position] + [n for n, _ in rec.ho_telescope])
                        for arg in [AtomExpr(-1, n) for n in ctor_env_names[: decl.param_count]] + rec.target_index_exprs
                    ],
                )
                for rec in decl.recursive_fields
            ],
        )
    if isinstance(decl, REliminatorDecl):
        return CEliminatorDecl(
            decl.name,
            decl.owner,
            restricted_expr_to_core(decl.motive_type, []),
            restricted_expr_to_core(decl.typ, []),
            decl.kind,
            decl.branch_order,
        )
    raise TypeError(f"unsupported restricted declaration: {type(decl).__name__}")


def _final_result_of_ctor_type(expr: Expr) -> Expr:
    current = expr
    while isinstance(current, PiExpr):
        current = current.codomain
    return current
