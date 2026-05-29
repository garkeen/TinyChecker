from __future__ import annotations

import unittest

from Implementation.gui_support import (
    AppExpr,
    AtomExpr,
    LambdaExpr,
    Param,
    analyze_source,
    show_expr,
)


class GuiSupportTests(unittest.TestCase):
    def test_show_expr_flattens_left_associative_application(self) -> None:
        expr = AppExpr(
            1,
            AppExpr(
                2,
                AppExpr(3, AtomExpr(4, "A"), AtomExpr(5, "B")),
                AtomExpr(6, "C"),
            ),
            AtomExpr(7, "D"),
        )
        self.assertEqual(show_expr(expr), "A B C D")

    def test_show_expr_parenthesizes_application_argument(self) -> None:
        expr = AppExpr(
            1,
            AtomExpr(2, "A"),
            AppExpr(3, AtomExpr(4, "B"), AtomExpr(5, "C")),
        )
        self.assertEqual(show_expr(expr), "A (B C)")

    def test_show_expr_uses_lambda_symbol_style(self) -> None:
        expr = LambdaExpr(1, Param(2, "x", AtomExpr(3, "T")), AtomExpr(4, "x"))
        self.assertEqual(show_expr(expr), "λx:T.x")

    def test_gap_focus_prefers_smallest_crossing_expr(self) -> None:
        src = "fun f (x:Type) (y:Type): Type { x y };"
        gap = src.index("x y") + 1
        analysis = analyze_source(src, gap)
        self.assertIsNotNone(analysis.target_expr)
        self.assertIsInstance(analysis.target_expr, AppExpr)

    def test_probe_reports_local_context_for_lambda_body(self) -> None:
        src = "fun f (x:Type): Type { x };"
        pos = src.rindex("x")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        self.assertEqual(
            [(entry.name, entry.typ) for entry in analysis.probe.local_context],
            [("x", "Type")],
        )
        self.assertEqual(analysis.probe.resolved_name, "local:x")

    def test_match_probe_reports_current_branch_fields_and_ih(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun pred (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [ihK] => k
  end
};
"""
        pos = src.rindex("k")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        self.assertEqual(len(analysis.probe.branch_stack), 1)
        frame = analysis.probe.branch_stack[-1]
        self.assertEqual(frame.constructor, "succ")
        self.assertEqual(frame.fields, ["k"])
        self.assertEqual(frame.ihs, ["ihK"])

    def test_nested_match_probe_keeps_outer_branch_path(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun nested (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [outerIH] =>
      match k as r in Nat return Nat with
      | zero => k
      | succ m [innerIH] => m
      end
  end
};
"""
        pos = src.rindex("m")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        self.assertEqual([frame.constructor for frame in analysis.probe.branch_stack], ["succ", "succ"])
        self.assertEqual(analysis.probe.branch_stack[0].ihs, ["outerIH"])
        self.assertEqual(analysis.probe.branch_stack[1].fields, ["m"])
        self.assertEqual(analysis.probe.branch_stack[1].ihs, ["innerIH"])

    def test_match_branch_context_survives_nested_let(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun nestedLet (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [ihK] =>
      let t:Nat = k in t
  end
};
"""
        pos = src.rindex("t")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        names = [entry.name for entry in analysis.probe.local_context]
        self.assertIn("t", names)
        self.assertIn("k", names)
        self.assertEqual(analysis.probe.branch_stack[-1].constructor, "succ")
        self.assertEqual(analysis.probe.branch_stack[-1].fields, ["k"])
        self.assertEqual(analysis.probe.branch_stack[-1].ihs, ["ihK"])

    def test_case_inside_match_keeps_outer_branch_and_inner_branch(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

sum Flag {
| leftFlag: Flag
| rightFlag: Flag
};

fun nestedCase (n:Nat) (f:Flag): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [ihK] =>
      case f as g in Flag return Nat of
      | leftFlag => k
      | rightFlag => ihK
      end
  end
};
"""
        pos = src.rindex("ihK")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        self.assertEqual([frame.constructor for frame in analysis.probe.branch_stack], ["succ", "rightFlag"])
        self.assertEqual(analysis.probe.branch_stack[0].fields, ["k"])
        self.assertEqual(analysis.probe.branch_stack[0].ihs, ["ihK"])
        self.assertEqual(analysis.probe.branch_stack[1].fields, [])
        self.assertEqual(analysis.probe.branch_stack[1].ihs, [])

    def test_match_inside_case_keeps_outer_branch_and_inner_branch(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

sum Flag {
| leftFlag: Flag
| rightFlag: Flag
};

fun caseMatch (f:Flag) (n:Nat): Nat {
  case f as g in Flag return Nat of
  | leftFlag =>
      match n as q in Nat return Nat with
      | zero => zero
      | succ k [ihK] => ihK
      end
  | rightFlag => n
  end
};
"""
        pos = src.rindex("ihK")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        self.assertEqual([frame.constructor for frame in analysis.probe.branch_stack], ["leftFlag", "succ"])
        self.assertEqual(analysis.probe.branch_stack[0].fields, [])
        self.assertEqual(analysis.probe.branch_stack[1].fields, ["k"])
        self.assertEqual(analysis.probe.branch_stack[1].ihs, ["ihK"])

    def test_let_binding_name_is_focusable(self) -> None:
        src = "fun f (x:Type): Type { let t:Type = x in t };"
        pos = src.index("t:Type")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.target_name_site)
        self.assertEqual(analysis.target_name_site.role, "let_name")
        self.assertIsNotNone(analysis.probe)
        self.assertEqual(analysis.probe.target_text, "t")
        self.assertEqual(analysis.probe.resolved_name, "binder:t")
        self.assertEqual(
            [(entry.name, entry.typ) for entry in analysis.probe.local_context],
            [("t", "Type"), ("x", "Type")],
        )

    def test_match_branch_field_name_is_focusable(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun pred (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [ihK] => k
  end
};
"""
        pos = src.index("k [")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.target_name_site)
        self.assertEqual(analysis.target_name_site.role, "branch_field")
        self.assertIsNotNone(analysis.probe)
        self.assertEqual(analysis.probe.target_text, "k")
        self.assertEqual(analysis.probe.resolved_name, "binder:k")
        self.assertEqual(analysis.probe.branch_stack[-1].constructor, "succ")

    def test_match_branch_ih_name_is_focusable(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun pred (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [ihK] => k
  end
};
"""
        pos = src.index("ihK")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.target_name_site)
        self.assertEqual(analysis.target_name_site.role, "branch_ih")
        self.assertIsNotNone(analysis.probe)
        self.assertEqual(analysis.probe.target_text, "ihK")
        self.assertEqual(analysis.probe.resolved_name, "binder:ihK")
        self.assertEqual(analysis.probe.branch_stack[-1].constructor, "succ")

    def test_nested_inner_ih_name_keeps_outer_branch_stack(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun nested (n:Nat): Nat {
  match n as q in Nat return Nat with
  | zero => zero
  | succ k [outerIH] =>
      match k as r in Nat return Nat with
      | zero => k
      | succ m [innerIH] => m
      end
  end
};
"""
        pos = src.rindex("innerIH")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.target_name_site)
        self.assertEqual(analysis.target_name_site.role, "branch_ih")
        self.assertEqual([frame.constructor for frame in analysis.probe.branch_stack], ["succ", "succ"])

    def test_list_match_ih_type_does_not_render_string_repr(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

inductive List (A:Type): Nat -> Type {
| nil: List A zero
| cons: (k:Nat) -> A -> List A k -> List A (succ k)
};

fun ListLength (A:Type) (n:Nat) (L:List A n): Nat {
  match L in List A n bind T k
  return Nat with
  | nil => zero
  | cons k a xs [ih] => succ ih
  end
};
"""
        pos = src.rindex("ih")
        analysis = analyze_source(src, pos)
        self.assertIsNotNone(analysis.probe)
        ih_entries = [entry for entry in analysis.probe.local_context if entry.name == "ih"]
        self.assertEqual(len(ih_entries), 1)
        self.assertNotIn("'T'", ih_entries[0].typ)


if __name__ == "__main__":
    unittest.main()
