from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class CTerm:
    pass


@dataclass(frozen=True)
class CVar(CTerm):
    index: int
    name: Optional[str] = None


@dataclass(frozen=True)
class CGlobal(CTerm):
    name: str


@dataclass(frozen=True)
class CType(CTerm):
    pass


@dataclass(frozen=True)
class CPi(CTerm):
    name: str
    domain: CTerm
    codomain: CTerm


@dataclass(frozen=True)
class CLam(CTerm):
    name: str
    param_type: CTerm
    body: CTerm


@dataclass(frozen=True)
class CApp(CTerm):
    func: CTerm
    arg: CTerm


@dataclass(frozen=True)
class CRecursiveFieldDecl:
    field_position: int
    ho_telescope: list[tuple[str, CTerm]]
    recursive_target_args: list[CTerm]


@dataclass(frozen=True)
class CDefinition:
    name: str
    typ: CTerm
    value: Optional[CTerm]
    kind: str


@dataclass(frozen=True)
class CTypeCtorDecl:
    name: str
    param_telescope: list[tuple[str, CTerm]]
    index_telescope: list[tuple[str, CTerm]]
    typ: CTerm
    kind: str
    family_kind: str
    constructor_names: list[str]


@dataclass(frozen=True)
class CDataCtorDecl:
    name: str
    owner: str
    typ: CTerm
    kind: str
    constructor_parameter_list: list[tuple[str, CTerm]]
    target_args: list[CTerm]
    recursive_fields: list[CRecursiveFieldDecl]


@dataclass(frozen=True)
class CEliminatorDecl:
    name: str
    owner: str
    motive_type: CTerm
    typ: CTerm
    kind: str
    branch_order: list[str]


CDecl = CDefinition | CTypeCtorDecl | CDataCtorDecl | CEliminatorDecl


def unfold_app(term: CTerm) -> tuple[CTerm, list[CTerm]]:
    args: list[CTerm] = []
    current = term
    while isinstance(current, CApp):
        args.append(current.arg)
        current = current.func
    args.reverse()
    return current, args


def mk_apps(head: CTerm, args: list[CTerm]) -> CTerm:
    term = head
    for arg in args:
        term = CApp(term, arg)
    return term
