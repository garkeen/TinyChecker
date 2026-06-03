from __future__ import annotations

from pathlib import Path

from .core import CGlobal
from .elaborator import elaborate_result
from .lexer import tokenize
from .parser import parse
from .reducer import Reducer
from .reducer import ConvStrategy
from .typechecker import check_program


def run_pipeline(source: str, *, conv_strategy: ConvStrategy = "whnf"):
    tokens = tokenize(source)
    decls = parse(tokens)
    elab = elaborate_result(decls)
    global_ctx = check_program(elab.core_decls, conv_strategy=conv_strategy)
    return tokens, decls, elab.core_decls, global_ctx


def run_pipeline_detailed(source: str, *, conv_strategy: ConvStrategy = "whnf"):
    tokens = tokenize(source)
    decls = parse(tokens)
    elab = elaborate_result(decls)
    global_ctx = check_program(elab.core_decls, conv_strategy=conv_strategy)
    return tokens, decls, elab.restricted_decls, elab.core_decls, global_ctx


def check_file(path: str | Path, *, conv_strategy: ConvStrategy = "whnf"):
    source = Path(path).read_text(encoding="utf-8")
    return run_pipeline(source, conv_strategy=conv_strategy)


def normal_form(term, global_ctx):
    return Reducer(global_ctx).nf(term)


def normal_form_of_global(name: str, global_ctx):
    return Reducer(global_ctx).nf(CGlobal(name))
