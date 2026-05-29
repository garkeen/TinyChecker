from __future__ import annotations

import unittest

from Implementation.restricted import RDefinition

from Test.support import ROOT, check_file, elaborate_program


class ExampleProgramsSuccessTests(unittest.TestCase):
    def test_main_examples_typecheck(self) -> None:
        for name in [
            "Nat.txt",
            "List.txt",
            "Tree.txt",
            "Sort.txt",
            "PropositionLogic.txt",
            "FirstOrderlogic.txt",
        ]:
            with self.subTest(example=name):
                _, _, _, global_ctx = check_file(ROOT / "Example" / name)
                self.assertGreater(len(global_ctx.globals), 0)

    def test_first_order_logic_registers_sum_and_product_kinds(self) -> None:
        _, _, _, global_ctx = check_file(ROOT / "Example" / "FirstOrderlogic.txt")
        self.assertEqual(global_ctx.inductives["Flag"].kind, "sum")
        self.assertEqual(global_ctx.inductives["ExistsUnit"].kind, "product")

    def test_example_decl_becomes_var_and_skips_conflicting_names(self) -> None:
        src = """
var example_1 : Type = Type;
example : Type = Type;
example : Type = Type;
"""
        elab = elaborate_program(src)
        defs = [decl for decl in elab.restricted_decls if isinstance(decl, RDefinition)]
        self.assertEqual([decl.name for decl in defs], ["example_1", "example_2", "example_3"])
        self.assertEqual([decl.kind for decl in defs], ["var", "var", "var"])

    def test_first_order_logic_projection_example_has_expected_global_names(self) -> None:
        _, _, _, global_ctx = check_file(ROOT / "Example" / "FirstOrderlogic.txt")
        self.assertIn("ExistsUnit.A", global_ctx.globals)
        self.assertIn("ExistsUnit.x", global_ctx.globals)
        self.assertEqual(global_ctx.globals["ExistsUnit.A"].kind, "projection")
        self.assertEqual(global_ctx.globals["ExistsUnit.x"].kind, "projection")


if __name__ == "__main__":
    unittest.main()
