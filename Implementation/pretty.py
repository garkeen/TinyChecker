from __future__ import annotations

from .core import CApp, CGlobal, CLam, CPi, CTerm, CType, CVar, unfold_app


def show_term(term: CTerm) -> str:
    if isinstance(term, CType):
        return "Type"
    if isinstance(term, CVar):
        return term.name or f"@{term.index}"
    if isinstance(term, CGlobal):
        return term.name
    if isinstance(term, CPi):
        return f"({term.name}:{show_term(term.domain)}) -> {show_term(term.codomain)}"
    if isinstance(term, CLam):
        return f"\\({term.name}:{show_term(term.param_type)}) => {show_term(term.body)}"
    if isinstance(term, CApp):
        head, args = unfold_app(term)
        pieces = [show_term(head)] + [show_term(arg) for arg in args]
        return "(" + " ".join(pieces) + ")"
    return repr(term)
