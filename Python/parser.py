from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .errors import ParseError
from .lexer import Token
from .surface import (
    AppExpr,
    ArrowExpr,
    AtomExpr,
    AxiomDecl,
    CaseBranch,
    CaseExpr,
    CtorDecl,
    Decl,
    EqExpr,
    EquationDecl,
    ExampleDecl,
    Expr,
    FieldDecl,
    FunDecl,
    InductiveDecl,
    LambdaExpr,
    LetExpr,
    MatchBranch,
    MatchExpr,
    Param,
    PiExpr,
    ProductDecl,
    ProductExpr,
    VarDecl,
    unfold_surface_app,
)

EXPR_START_KINDS = {
    "LET",
    "LAMBDA",
    "MATCH",
    "CASE",
    "LBRACKET",
    "LPAREN",
    "IDENT",
    "DOT",
    "TYPE",
}


@dataclass(frozen=True)
class ParseArtifacts:
    decls: list[Decl]
    node_spans: dict[int, tuple[int, int]]
    name_sites: list["NameSite"]


@dataclass(frozen=True)
class NameSite:
    site_id: int
    name: str
    role: str
    owner_surface_id: int
    token_span: tuple[int, int]
    parent_expr_id: Optional[int]
    index: int


@dataclass
class Parser:
    tokens: list[Token]
    pos: int = 0
    next_surface_id: int = 1
    next_site_id: int = 1
    node_spans: dict[int, tuple[int, int]] = field(default_factory=dict)
    name_sites: list[NameSite] = field(default_factory=list)

    def allocate(self) -> int:
        sid = self.next_surface_id
        self.next_surface_id += 1
        return sid

    def record_span(self, node, start: int, end: int):
        self.node_spans[node.surface_id] = (start, end)
        return node

    def allocate_site(self) -> int:
        site_id = self.next_site_id
        self.next_site_id += 1
        return site_id

    def record_name_site(
        self,
        name: Optional[str],
        role: str,
        owner_surface_id: int,
        start: int,
        end: int,
        *,
        parent_expr_id: Optional[int],
        index: int,
    ) -> None:
        if name is None:
            return
        self.name_sites.append(
            NameSite(
                site_id=self.allocate_site(),
                name=name,
                role=role,
                owner_surface_id=owner_surface_id,
                token_span=(start, end),
                parent_expr_id=parent_expr_id,
                index=index,
            )
        )

    def current(self) -> Token:
        return self.tokens[self.pos]

    def peek(self, offset: int = 1) -> Token:
        index = min(self.pos + offset, len(self.tokens) - 1)
        return self.tokens[index]

    def at(self, kind: str) -> bool:
        return self.current().kind == kind

    def consume(self, kind: str) -> Token:
        token = self.current()
        if token.kind != kind:
            raise ParseError(
                f"expected {kind}, got {token.kind} at {token.row}:{token.col}"
            )
        self.pos += 1
        return token

    def maybe(self, kind: str) -> Optional[Token]:
        if self.at(kind):
            return self.consume(kind)
        return None

    def parse(self) -> list[Decl]:
        decls: list[Decl] = []
        while not self.at("EOF"):
            decls.append(self.parse_decl())
            while self.maybe("SEMI") is not None:
                pass
        return decls

    def parse_decl(self) -> Decl:
        start = self.pos
        kind = self.current().kind
        if kind in {"VAR", "CLAIM"}:
            return self.record_span(self.parse_var_decl(), start, self.pos)
        if kind in {"FUN", "THEOREM"}:
            return self.record_span(self.parse_fun_decl(), start, self.pos)
        if kind in {"INDUCTIVE", "SUM"}:
            return self.record_span(self.parse_inductive_decl(), start, self.pos)
        if kind == "PRODUCT":
            return self.record_span(self.parse_product_decl(), start, self.pos)
        if kind == "AXIOM":
            return self.record_span(self.parse_axiom_decl(), start, self.pos)
        if kind == "EXAMPLE":
            return self.record_span(self.parse_example_decl(), start, self.pos)
        if kind == "IDENT":
            return self.record_span(self.parse_equation_decl(), start, self.pos)
        token = self.current()
        raise ParseError(f"unexpected declaration at {token.row}:{token.col}")

    def parse_var_decl(self) -> VarDecl:
        head = self.consume(self.current().kind)
        name = self.consume("IDENT").text
        self.consume("COLON")
        typ = self.parse_expr(stop={"EQ"})
        self.consume("EQ")
        value = self.parse_expr(stop={"SEMI", "EOF"})
        self.consume("SEMI")
        return VarDecl(self.allocate(), head.text, name, typ, value)

    def parse_fun_decl(self) -> FunDecl:
        head = self.consume(self.current().kind)
        name = self.consume("IDENT").text
        params: list[Param] = []
        while self.at("LPAREN"):
            params.append(self.parse_param())
        self.consume("COLON")
        ret_type = self.parse_expr(stop={"LBRACE"})
        self.consume("LBRACE")
        body = self.parse_expr(stop={"RBRACE"})
        self.consume("RBRACE")
        self.maybe("SEMI")
        return FunDecl(self.allocate(), head.text, name, params, ret_type, body)

    def parse_ctor_block(self) -> list[CtorDecl]:
        ctors: list[CtorDecl] = []
        self.consume("LBRACE")
        if self.at("RBRACE"):
            self.consume("RBRACE")
            return ctors
        self.consume("BAR")
        while not self.at("RBRACE"):
            start = self.pos
            name = self.consume("IDENT").text
            self.consume("COLON")
            typ = self.parse_expr(stop={"BAR", "RBRACE"})
            ctors.append(self.record_span(CtorDecl(self.allocate(), name, typ), start, self.pos))
            if self.at("BAR"):
                self.consume("BAR")
                continue
            if self.at("RBRACE"):
                break
            token = self.current()
            raise ParseError(f"invalid constructor separator at {token.row}:{token.col}")
        self.consume("RBRACE")
        return ctors

    def parse_inductive_decl(self) -> InductiveDecl:
        head = self.consume(self.current().kind)
        name = self.consume("IDENT").text
        params: list[Param] = []
        if head.kind == "INDUCTIVE":
            while self.at("LPAREN"):
                params.append(self.parse_param())
        arity = None
        if self.maybe("COLON") is not None:
            arity = self.parse_expr(stop={"LBRACE"})
        ctors = self.parse_ctor_block()
        self.maybe("SEMI")
        return InductiveDecl(self.allocate(), head.text, name, params, arity, ctors)

    def parse_product_decl(self) -> ProductDecl:
        self.consume("PRODUCT")
        name = self.consume("IDENT").text
        fields: list[FieldDecl] = []
        self.consume("LBRACE")
        while not self.at("RBRACE"):
            start = self.pos
            field_name = self.consume("IDENT").text
            self.consume("COLON")
            field_ty = self.parse_expr(stop={"COMMA", "RBRACE"})
            fields.append(self.record_span(FieldDecl(self.allocate(), field_name, field_ty), start, self.pos))
            if self.at("COMMA"):
                self.consume("COMMA")
            else:
                break
        self.consume("RBRACE")
        self.maybe("SEMI")
        return ProductDecl(self.allocate(), name, fields)

    def parse_axiom_decl(self) -> AxiomDecl:
        self.consume("AXIOM")
        name = self.consume("IDENT").text
        self.consume("COLON")
        typ = self.parse_expr(stop={"SEMI"})
        self.consume("SEMI")
        return AxiomDecl(self.allocate(), name, typ)

    def parse_example_decl(self) -> ExampleDecl:
        self.consume("EXAMPLE")
        self.consume("COLON")
        typ = self.parse_expr(stop={"EQ"})
        self.consume("EQ")
        value = self.parse_expr(stop={"SEMI", "EOF"})
        self.maybe("SEMI")
        return ExampleDecl(self.allocate(), typ, value)

    def parse_equation_decl(self) -> EquationDecl:
        name = self.consume("IDENT").text
        self.consume("COLON")
        typ = self.parse_expr(stop={"COMMA"})
        self.consume("COMMA")
        name2 = self.consume("IDENT").text
        if name != name2:
            raise ParseError(f"equation name mismatch: {name} vs {name2}")
        params: list[Optional[str]] = []
        while self.at("IDENT"):
            params.append(self.parse_bind_name())
        self.consume("EQ")
        value = self.parse_expr(stop={"SEMI", "EOF"})
        self.maybe("SEMI")
        return EquationDecl(self.allocate(), name, typ, params, value)

    def parse_expr(self, stop: set[str]) -> Expr:
        start = self.pos
        left = self.parse_app(stop | {"ARROW"})
        if self.at("ARROW") and "ARROW" not in stop:
            self.consume("ARROW")
            right = self.parse_expr(stop)
            return self.record_span(ArrowExpr(self.allocate(), left, right), start, self.pos)
        return left

    def parse_app(self, stop: set[str]) -> Expr:
        start = self.pos
        term = self.parse_no_app(stop)
        while self.current().kind in EXPR_START_KINDS and self.current().kind not in stop:
            arg = self.parse_no_app(stop)
            term = self.record_span(AppExpr(self.allocate(), term, arg), start, self.pos)
        return term

    def parse_no_app(self, stop: set[str]) -> Expr:
        kind = self.current().kind
        if kind in stop:
            token = self.current()
            raise ParseError(f"unexpected token {kind} at {token.row}:{token.col}")
        if kind == "LET":
            return self.parse_let_expr()
        if kind == "LAMBDA":
            return self.parse_lambda_expr()
        if kind == "MATCH":
            return self.parse_match_expr()
        if kind == "CASE":
            return self.parse_case_expr()
        if kind == "LBRACKET":
            return self.parse_eq_expr()
        if kind == "LPAREN":
            if self.peek(1).kind == "IDENT" and self.peek(2).kind == "COLON":
                return self.parse_pi_expr()
            return self.parse_paren_expr()
        if kind == "IDENT":
            if self.peek(1).kind == "LANGLE":
                return self.parse_product_expr()
            return self.parse_atom()
        if kind in {"DOT", "TYPE"}:
            return self.parse_atom()
        token = self.current()
        raise ParseError(f"invalid expression at {token.row}:{token.col}")

    def parse_param(self) -> Param:
        start = self.pos
        self.consume("LPAREN")
        name = self.parse_bind_name()
        self.consume("COLON")
        typ = self.parse_expr(stop={"RPAREN"})
        self.consume("RPAREN")
        return self.record_span(Param(self.allocate(), name, typ), start, self.pos)

    def parse_bind_name(self) -> Optional[str]:
        token = self.consume("IDENT")
        return None if token.text == "_" else token.text

    def parse_bind_name_with_span(self) -> tuple[Optional[str], tuple[int, int]]:
        start = self.pos
        token = self.consume("IDENT")
        name = None if token.text == "_" else token.text
        return name, (start, self.pos)

    def parse_atom(self) -> Expr:
        start = self.pos
        token = self.current()
        if token.kind not in {"IDENT", "DOT", "TYPE"}:
            raise ParseError(f"expected atom at {token.row}:{token.col}")
        self.pos += 1
        if token.kind == "IDENT" and token.text == "_":
            raise ParseError(f"'_' is not allowed as an expression at {token.row}:{token.col}")
        return self.record_span(AtomExpr(self.allocate(), token.text), start, self.pos)

    def parse_paren_expr(self) -> Expr:
        self.consume("LPAREN")
        expr = self.parse_expr(stop={"RPAREN"})
        self.consume("RPAREN")
        return expr

    def parse_pi_expr(self) -> Expr:
        start = self.pos
        self.consume("LPAREN")
        name = self.parse_bind_name()
        self.consume("COLON")
        domain = self.parse_expr(stop={"RPAREN"})
        self.consume("RPAREN")
        self.consume("ARROW")
        codomain = self.parse_expr(stop=set())
        return self.record_span(PiExpr(self.allocate(), name, domain, codomain), start, self.pos)

    def parse_lambda_expr(self) -> Expr:
        start = self.pos
        self.consume("LAMBDA")
        param = self.parse_param()
        self.consume("DARROW")
        body = self.parse_expr(stop=set())
        return self.record_span(LambdaExpr(self.allocate(), param, body), start, self.pos)

    def parse_let_expr(self) -> Expr:
        start = self.pos
        surface_id = self.allocate()
        self.consume("LET")
        name, name_span = self.parse_bind_name_with_span()
        self.record_name_site(
            name,
            "let_name",
            surface_id,
            name_span[0],
            name_span[1],
            parent_expr_id=surface_id,
            index=0,
        )
        self.consume("COLON")
        typ = self.parse_expr(stop={"EQ"})
        self.consume("EQ")
        value = self.parse_expr(stop={"IN"})
        self.consume("IN")
        body = self.parse_expr(stop=set())
        return self.record_span(LetExpr(surface_id, name, typ, value, body), start, self.pos)

    def parse_eq_expr(self) -> Expr:
        start = self.pos
        self.consume("LBRACKET")
        typ = self.parse_expr(stop={"RBRACKET"})
        self.consume("RBRACKET")
        lhs = self.parse_expr(stop={"EQEQ"})
        self.consume("EQEQ")
        rhs = self.parse_expr(stop=set())
        return self.record_span(EqExpr(self.allocate(), typ, lhs, rhs), start, self.pos)

    def parse_match_branch(self) -> MatchBranch:
        start = self.pos
        surface_id = self.allocate()
        self.consume("BAR")
        ctor = self.consume("IDENT").text
        fields: list[Optional[str]] = []
        while self.at("IDENT"):
            field_name, field_span = self.parse_bind_name_with_span()
            self.record_name_site(
                field_name,
                "branch_field",
                surface_id,
                field_span[0],
                field_span[1],
                parent_expr_id=None,
                index=len(fields),
            )
            fields.append(field_name)
        ihs: list[Optional[str]] = []
        if self.maybe("LBRACKET") is not None:
            while not self.at("RBRACKET"):
                ih_name, ih_span = self.parse_bind_name_with_span()
                self.record_name_site(
                    ih_name,
                    "branch_ih",
                    surface_id,
                    ih_span[0],
                    ih_span[1],
                    parent_expr_id=None,
                    index=len(ihs),
                )
                ihs.append(ih_name)
            self.consume("RBRACKET")
        self.consume("DARROW")
        body = self.parse_expr(stop={"BAR", "END"})
        return self.record_span(MatchBranch(surface_id, ctor, fields, ihs, body), start, self.pos)

    def parse_case_branch(self) -> CaseBranch:
        start = self.pos
        surface_id = self.allocate()
        self.consume("BAR")
        ctor = self.consume("IDENT").text
        fields: list[Optional[str]] = []
        while self.at("IDENT"):
            field_name, field_span = self.parse_bind_name_with_span()
            self.record_name_site(
                field_name,
                "branch_field",
                surface_id,
                field_span[0],
                field_span[1],
                parent_expr_id=None,
                index=len(fields),
            )
            fields.append(field_name)
        self.consume("DARROW")
        body = self.parse_expr(stop={"BAR", "END"})
        return self.record_span(CaseBranch(surface_id, ctor, fields, body), start, self.pos)

    def parse_match_expr(self) -> Expr:
        start = self.pos
        self.consume("MATCH")
        scrutinee = self.parse_expr(stop={"AS", "IN"})
        alias = None
        if self.maybe("AS") is not None:
            alias = self.parse_bind_name()
        self.consume("IN")
        family_expr = self.parse_expr(stop={"BIND", "RETURN"})
        head, args = unfold_surface_app(family_expr)
        if not isinstance(head, AtomExpr):
            token = self.current()
            raise ParseError(f"match family head must be an atom near {token.row}:{token.col}")
        bind_names: list[Optional[str]] = []
        if self.maybe("BIND") is not None:
            while self.at("IDENT"):
                bind_names.append(self.parse_bind_name())
        self.consume("RETURN")
        motive = self.parse_expr(stop={"WITH"})
        self.consume("WITH")
        branches: list[MatchBranch] = []
        while self.at("BAR"):
            branches.append(self.parse_match_branch())
        self.consume("END")
        return self.record_span(
            MatchExpr(
                self.allocate(),
                scrutinee,
                alias,
                head.text,
                args,
                bind_names,
                motive,
                branches,
            ),
            start,
            self.pos,
        )

    def parse_case_expr(self) -> Expr:
        start = self.pos
        self.consume("CASE")
        scrutinee = self.parse_expr(stop={"AS", "IN"})
        alias = None
        if self.maybe("AS") is not None:
            alias = self.parse_bind_name()
        self.consume("IN")
        family_expr = self.parse_expr(stop={"BIND", "RETURN"})
        head, args = unfold_surface_app(family_expr)
        if not isinstance(head, AtomExpr):
            token = self.current()
            raise ParseError(f"case family head must be an atom near {token.row}:{token.col}")
        bind_names: list[Optional[str]] = []
        if self.maybe("BIND") is not None:
            while self.at("IDENT"):
                bind_names.append(self.parse_bind_name())
        self.consume("RETURN")
        motive = self.parse_expr(stop={"OF"})
        self.consume("OF")
        branches: list[CaseBranch] = []
        while self.at("BAR"):
            branches.append(self.parse_case_branch())
        self.consume("END")
        return self.record_span(
            CaseExpr(
                self.allocate(),
                scrutinee,
                alias,
                head.text,
                args,
                bind_names,
                motive,
                branches,
            ),
            start,
            self.pos,
        )

    def parse_product_expr(self) -> Expr:
        start = self.pos
        type_name = self.consume("IDENT").text
        self.consume("LANGLE")
        args: list[Expr] = []
        if not self.at("RANGLE"):
            args.append(self.parse_expr(stop={"COMMA", "RANGLE"}))
            while self.at("COMMA"):
                self.consume("COMMA")
                args.append(self.parse_expr(stop={"COMMA", "RANGLE"}))
        self.consume("RANGLE")
        return self.record_span(ProductExpr(self.allocate(), type_name, args), start, self.pos)


def parse(tokens: list[Token]) -> list[Decl]:
    return Parser(tokens).parse()


def parse_with_metadata(tokens: list[Token]) -> ParseArtifacts:
    parser = Parser(tokens)
    decls = parser.parse()
    return ParseArtifacts(decls, parser.node_spans, parser.name_sites)
