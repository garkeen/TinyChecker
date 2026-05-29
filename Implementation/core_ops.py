from __future__ import annotations

from .core import CApp, CGlobal, CLam, CPi, CTerm, CType, CVar


def shift(term: CTerm, amount: int, cutoff: int = 0) -> CTerm:
    if amount == 0:
        return term
    if isinstance(term, CVar):
        if term.index >= cutoff:
            return CVar(term.index + amount, term.name)
        return term
    if isinstance(term, CGlobal | CType):
        return term
    if isinstance(term, CPi):
        return CPi(
            term.name,
            shift(term.domain, amount, cutoff),
            shift(term.codomain, amount, cutoff + 1),
        )
    if isinstance(term, CLam):
        return CLam(
            term.name,
            shift(term.param_type, amount, cutoff),
            shift(term.body, amount, cutoff + 1),
        )
    if isinstance(term, CApp):
        return CApp(shift(term.func, amount, cutoff), shift(term.arg, amount, cutoff))
    raise TypeError(f"unknown term: {term}")


def subst(term: CTerm, index: int, replacement: CTerm, cutoff: int = 0) -> CTerm:
    if isinstance(term, CVar):
        target = index + cutoff
        if term.index == target:
            return shift(replacement, cutoff)
        if term.index > target:
            return CVar(term.index - 1, term.name)
        return term
    if isinstance(term, CGlobal | CType):
        return term
    if isinstance(term, CPi):
        return CPi(
            term.name,
            subst(term.domain, index, replacement, cutoff),
            subst(term.codomain, index, replacement, cutoff + 1),
        )
    if isinstance(term, CLam):
        return CLam(
            term.name,
            subst(term.param_type, index, replacement, cutoff),
            subst(term.body, index, replacement, cutoff + 1),
        )
    if isinstance(term, CApp):
        return CApp(
            subst(term.func, index, replacement, cutoff),
            subst(term.arg, index, replacement, cutoff),
        )
    raise TypeError(f"unknown term: {term}")


def instantiate(body: CTerm, arg: CTerm) -> CTerm:
    return subst(body, 0, arg)


def instantiate_many(body: CTerm, args: list[CTerm]) -> CTerm:
    current = body
    for arg in reversed(args):
        current = instantiate(current, arg)
    return current


def instantiate_env(term: CTerm, env: list[CTerm], depth: int = 0) -> CTerm:
    if not env:
        return term
    if isinstance(term, CVar):
        if term.index < depth:
            return term
        offset = term.index - depth
        if offset < len(env):
            return shift(env[offset], depth)
        return CVar(term.index - len(env), term.name)
    if isinstance(term, CGlobal | CType):
        return term
    if isinstance(term, CPi):
        return CPi(
            term.name,
            instantiate_env(term.domain, env, depth),
            instantiate_env(term.codomain, env, depth + 1),
        )
    if isinstance(term, CLam):
        return CLam(
            term.name,
            instantiate_env(term.param_type, env, depth),
            instantiate_env(term.body, env, depth + 1),
        )
    if isinstance(term, CApp):
        return CApp(
            instantiate_env(term.func, env, depth),
            instantiate_env(term.arg, env, depth),
        )
    raise TypeError(f"unknown term: {term}")
