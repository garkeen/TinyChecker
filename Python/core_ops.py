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
        new_domain = shift(term.domain, amount, cutoff)
        new_codomain = shift(term.codomain, amount, cutoff + 1)
        if new_domain is term.domain and new_codomain is term.codomain:
            return term
        return CPi(term.name, new_domain, new_codomain)
    if isinstance(term, CLam):
        new_param_type = shift(term.param_type, amount, cutoff)
        new_body = shift(term.body, amount, cutoff + 1)
        if new_param_type is term.param_type and new_body is term.body:
            return term
        return CLam(term.name, new_param_type, new_body)
    if isinstance(term, CApp):
        new_func = shift(term.func, amount, cutoff)
        new_arg = shift(term.arg, amount, cutoff)
        if new_func is term.func and new_arg is term.arg:
            return term
        return CApp(new_func, new_arg)
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
        new_domain = subst(term.domain, index, replacement, cutoff)
        new_codomain = subst(term.codomain, index, replacement, cutoff + 1)
        if new_domain is term.domain and new_codomain is term.codomain:
            return term
        return CPi(term.name, new_domain, new_codomain)
    if isinstance(term, CLam):
        new_param_type = subst(term.param_type, index, replacement, cutoff)
        new_body = subst(term.body, index, replacement, cutoff + 1)
        if new_param_type is term.param_type and new_body is term.body:
            return term
        return CLam(term.name, new_param_type, new_body)
    if isinstance(term, CApp):
        new_func = subst(term.func, index, replacement, cutoff)
        new_arg = subst(term.arg, index, replacement, cutoff)
        if new_func is term.func and new_arg is term.arg:
            return term
        return CApp(new_func, new_arg)
    raise TypeError(f"unknown term: {term}")


def instantiate(body: CTerm, arg: CTerm) -> CTerm:
    if isinstance(body, CVar) and body.index == 0:
        return arg
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
        new_domain = instantiate_env(term.domain, env, depth)
        new_codomain = instantiate_env(term.codomain, env, depth + 1)
        if new_domain is term.domain and new_codomain is term.codomain:
            return term
        return CPi(term.name, new_domain, new_codomain)
    if isinstance(term, CLam):
        new_param_type = instantiate_env(term.param_type, env, depth)
        new_body = instantiate_env(term.body, env, depth + 1)
        if new_param_type is term.param_type and new_body is term.body:
            return term
        return CLam(term.name, new_param_type, new_body)
    if isinstance(term, CApp):
        new_func = instantiate_env(term.func, env, depth)
        new_arg = instantiate_env(term.arg, env, depth)
        if new_func is term.func and new_arg is term.arg:
            return term
        return CApp(new_func, new_arg)
    raise TypeError(f"unknown term: {term}")
