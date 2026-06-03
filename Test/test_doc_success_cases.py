from __future__ import annotations

import unittest

from Codes.surface import ArrowExpr, EqExpr

from Test.support import parse_single_expr, typecheck_source


class DocumentSuccessCaseTests(unittest.TestCase):
    def test_eq_expr_right_side_consumes_full_expr(self) -> None:
        expr = parse_single_expr("[Nat] zero == zero -> Nat")
        self.assertIsInstance(expr, EqExpr)
        self.assertIsInstance(expr.rhs, ArrowExpr)

    def test_case_on_sum_typechecks(self) -> None:
        src = """
sum Flag {
| leftFlag: Flag
| rightFlag: Flag
};

fun flip (x:Flag): Flag {
  case x
  in Flag
  return Flag
  of
  | leftFlag => rightFlag
  | rightFlag => leftFlag
  end
};
"""
        _, global_ctx = typecheck_source(src)
        self.assertEqual(global_ctx.inductives["Flag"].kind, "sum")

    def test_product_generates_constructor_eliminator_and_projections(self) -> None:
        src = """
product Pair {
  A: Type,
  x: A
};

var p : Pair = Pair<Type, Type>;
claim outA : Type = Pair.A p;
claim outX : Pair.A p = Pair.x p;
"""
        elab, global_ctx = typecheck_source(src)
        names = {decl.name for decl in elab.core_decls}
        self.assertIn("Pair.mk", names)
        self.assertIn("Pair.elim", names)
        self.assertIn("Pair.A", names)
        self.assertIn("Pair.x", names)
        self.assertEqual(global_ctx.inductives["Pair"].kind, "product")

    def test_match_without_explicit_ih_names_is_allowed(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun pred (n:Nat): Nat {
  match n
  in Nat
  return Nat
  with
  | zero => zero
  | succ k => k
  end
};
"""
        typecheck_source(src)

    def test_dot_on_product_projection_is_allowed(self) -> None:
        src = """
inductive Unit {
| unit: Unit
};

product ExistsUnit {
  A: Type,
  x: A
};

var packed: ExistsUnit = ExistsUnit<Unit, unit>;
claim outX : ExistsUnit.A packed = ExistsUnit.x packed;
"""
        _, global_ctx = typecheck_source(src)
        self.assertIn("ExistsUnit.A", global_ctx.globals)
        self.assertIn("ExistsUnit.x", global_ctx.globals)


if __name__ == "__main__":
    unittest.main()
