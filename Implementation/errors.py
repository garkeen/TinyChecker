from __future__ import annotations


class TinyCheckerError(Exception):
    """Base error for the reconstructed TinyChecker pipeline."""


class LexError(TinyCheckerError):
    pass


class ParseError(TinyCheckerError):
    pass


class ElabError(TinyCheckerError):
    pass


class TypeCheckError(TinyCheckerError):
    pass


class ReducerError(TinyCheckerError):
    pass
