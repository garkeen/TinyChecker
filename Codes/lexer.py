from __future__ import annotations

from dataclasses import dataclass

from .errors import LexError

TAB_WIDTH = 2

KEYWORDS = {
    "var": "VAR",
    "fun": "FUN",
    "claim": "CLAIM",
    "theorem": "THEOREM",
    "inductive": "INDUCTIVE",
    "sum": "SUM",
    "product": "PRODUCT",
    "axiom": "AXIOM",
    "example": "EXAMPLE",
    "let": "LET",
    "in": "IN",
    "match": "MATCH",
    "case": "CASE",
    "as": "AS",
    "bind": "BIND",
    "return": "RETURN",
    "with": "WITH",
    "of": "OF",
    "end": "END",
}

TYPE_KEYWORDS = {"Type", "Prop"}
MULTI_CHAR = {"=>": "DARROW", "->": "ARROW", "==": "EQEQ"}
SINGLE_CHAR = {
    "{": "LBRACE",
    "}": "RBRACE",
    "(": "LPAREN",
    ")": "RPAREN",
    "[": "LBRACKET",
    "]": "RBRACKET",
    "<": "LANGLE",
    ">": "RANGLE",
    ":": "COLON",
    "=": "EQ",
    "\\": "LAMBDA",
    ";": "SEMI",
    ",": "COMMA",
    "|": "BAR",
}


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    row: int
    col: int
    start_pos: int = -1
    end_pos: int = -1


class Lexer:
    def __init__(self, source: str) -> None:
        self.source = source
        self.pos = 0
        self.row = 1
        self.col = 1
        self.paren = 0
        self.bracket = 0
        self.angle = 0

    def error(self, message: str) -> LexError:
        return LexError(f"{message} at {self.row}:{self.col}")

    def current(self) -> str:
        return self.source[self.pos] if self.pos < len(self.source) else ""

    def peek(self, offset: int = 1) -> str:
        index = self.pos + offset
        return self.source[index] if index < len(self.source) else ""

    def advance(self, text: str) -> None:
        for ch in text:
            self.pos += 1
            if ch == "\n":
                self.row += 1
                self.col = 1
            elif ch == "\t":
                self.col += TAB_WIDTH
            else:
                self.col += 1

    def skip_whitespace(self) -> None:
        while self.pos < len(self.source):
            ch = self.current()
            if ch == " " or ch == "\r":
                self.advance(ch)
            elif ch == "\t":
                self.advance(ch)
            elif ch == "\n":
                self.advance(ch)
            elif ch == "#":
                while self.pos < len(self.source) and self.current() != "\n":
                    self.advance(self.current())
            else:
                return

    def lex_ident_or_keyword(self) -> Token:
        row, col = self.row, self.col
        start = self.pos
        while self.pos < len(self.source):
            ch = self.current()
            if ch.isalnum() or ch == "_":
                self.advance(ch)
            else:
                break
        first = self.source[start:self.pos]
        if self.current() == ".":
            dot_row, dot_col = self.row, self.col
            self.advance(".")
            if not (self.current().isalpha() or self.current() == "_"):
                raise LexError(f"invalid dot identifier at {dot_row}:{dot_col}")
            second_start = self.pos
            while self.pos < len(self.source):
                ch = self.current()
                if ch.isalnum() or ch == "_":
                    self.advance(ch)
                else:
                    break
            second = self.source[second_start:self.pos]
            if self.current() == ".":
                raise self.error("only one dot is allowed in DOT identifiers")
            return Token("DOT", f"{first}.{second}", row, col, start, self.pos)
        if first in TYPE_KEYWORDS:
            return Token("TYPE", first, row, col, start, self.pos)
        if first in KEYWORDS:
            return Token(KEYWORDS[first], first, row, col, start, self.pos)
        return Token("IDENT", first, row, col, start, self.pos)

    def lex_single(self) -> Token:
        ch = self.current()
        row, col = self.row, self.col
        start = self.pos
        kind = SINGLE_CHAR[ch]
        self.advance(ch)
        if ch == "(":
            self.paren += 1
        elif ch == ")":
            self.paren -= 1
            if self.paren < 0:
                raise self.error("unmatched ')'")
        elif ch == "[":
            self.bracket += 1
        elif ch == "]":
            self.bracket -= 1
            if self.bracket < 0:
                raise self.error("unmatched ']'")
        elif ch == "<":
            self.angle += 1
        elif ch == ">":
            self.angle -= 1
            if self.angle < 0:
                raise self.error("unmatched '>'")
        return Token(kind, ch, row, col, start, self.pos)

    def tokenize(self) -> list[Token]:
        tokens: list[Token] = []
        while self.pos < len(self.source):
            self.skip_whitespace()
            if self.pos >= len(self.source):
                break
            row, col = self.row, self.col
            start = self.pos
            two = self.source[self.pos:self.pos + 2]
            if two in MULTI_CHAR:
                self.advance(two)
                tokens.append(Token(MULTI_CHAR[two], two, row, col, start, self.pos))
                continue
            ch = self.current()
            if ch in SINGLE_CHAR:
                tokens.append(self.lex_single())
                continue
            if ch.isalpha() or ch == "_":
                tokens.append(self.lex_ident_or_keyword())
                continue
            raise self.error(f"unexpected character {ch!r}")
        if self.paren != 0 or self.bracket != 0 or self.angle != 0:
            raise self.error("unbalanced delimiters")
        tokens.append(Token("EOF", "", self.row, self.col, self.pos, self.pos))
        return tokens


def tokenize(source: str) -> list[Token]:
    return Lexer(source).tokenize()
