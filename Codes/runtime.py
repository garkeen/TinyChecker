from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .core import CRecursiveFieldDecl, CTerm


@dataclass
class GlobalEntry:
    name: str
    typ: CTerm
    value: Optional[CTerm]
    kind: str


@dataclass
class InductiveInfo:
    name: str
    kind: str
    param_count: int
    index_count: int
    index_telescope: list[tuple[str, CTerm]]
    constructor_names: list[str]
    eliminator_name: str


@dataclass
class ConstructorInfo:
    name: str
    owner: str
    typ: CTerm
    param_count: int
    fields: list[tuple[str, CTerm]]
    recursive_fields: list[CRecursiveFieldDecl]
    target_args: list[CTerm]


@dataclass
class EliminatorInfo:
    name: str
    owner: str
    typ: CTerm
    branch_order: list[str]
    kind: str


@dataclass
class TcGlobalContext:
    globals: dict[str, GlobalEntry] = field(default_factory=dict)
    inductives: dict[str, InductiveInfo] = field(default_factory=dict)
    constructors: dict[str, ConstructorInfo] = field(default_factory=dict)
    eliminators: dict[str, EliminatorInfo] = field(default_factory=dict)
