from __future__ import annotations

import unittest

from Test.support import ROOT, check_file, typecheck_source


class ConversionStrategyTests(unittest.TestCase):
    def test_both_strategies_typecheck_basic_program(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun idNat (n:Nat): Nat {
  n
};

claim keep : Nat = idNat zero;
"""
        for strategy in ["greedy", "whnf", "whnfv2"]:
            with self.subTest(strategy=strategy):
                _, global_ctx = typecheck_source(src, conv_strategy=strategy)
                self.assertIn("keep", global_ctx.globals)

    def test_example_file_accepts_all_strategies(self) -> None:
        for strategy in ["greedy", "whnf", "whnfv2"]:
            with self.subTest(strategy=strategy):
                _, _, _, global_ctx = check_file(ROOT / "Example" / "Nat.pind", conv_strategy=strategy)
                self.assertIn("Nat", global_ctx.globals)


if __name__ == "__main__":
    unittest.main()
