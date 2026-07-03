from __future__ import annotations

from pathlib import Path

from Codes.elaborator import elaborate_result
from Codes.errors import ElabError, LexError, ParseError, ReducerError, TypeCheckError
from Codes.lexer import tokenize
from Codes.parser import Parser, parse
from Codes.pipeline import check_file
from Codes.reducer import ConvStrategy
from Codes.typechecker import check_program


ROOT = Path(__file__).resolve().parent.parent


def parse_program(source: str):
    return parse(tokenize(source))


def elaborate_program(source: str):
    return elaborate_result(parse_program(source))


def typecheck_source(source: str, *, conv_strategy: ConvStrategy = "whnf"):
    elab = elaborate_program(source)
    global_ctx = check_program(elab.core_decls, conv_strategy=conv_strategy)
    return elab, global_ctx


def parse_single_expr(source: str):
    parser = Parser(tokenize(source))
    expr = parser.parse_expr(stop={"EOF"})
    parser.consume("EOF")
    return expr


__all__ = [
    "ROOT",
    "ElabError",
    "LexError",
    "ParseError",
    "ReducerError",
    "TypeCheckError",
    "check_file",
    "elaborate_program",
    "parse_program",
    "parse_single_expr",
    "typecheck_source",
]
