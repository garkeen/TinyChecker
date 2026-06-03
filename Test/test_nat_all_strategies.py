"""Manual test: run Example/Nat.txt with all three conversion strategies.

Usage:
    python -m Test.test_nat_all_strategies
"""

from __future__ import annotations

import io
import sys
import time
import unittest
from pathlib import Path

from Codes.core import CDefinition
from Codes.elaborator import elaborate_result
from Codes.lexer import tokenize
from Codes.parser import parse
from Codes.reducer import ConvStrategy
from Codes.typechecker import check_program

ROOT = Path(__file__).resolve().parent.parent
NAT_PATH = ROOT / "Example" / "Nat.txt"


def run_nat(strategy: ConvStrategy) -> dict:
    source = NAT_PATH.read_text(encoding="utf-8")
    t0 = time.perf_counter()

    try:
        tokens = tokenize(source)
        decls = parse(tokens)
        elab = elaborate_result(decls)
        global_ctx = check_program(elab.core_decls, conv_strategy=strategy)
        elapsed = time.perf_counter() - t0

        theorems = []
        claims = []
        funs = []
        for decl in elab.core_decls:
            if isinstance(decl, CDefinition):
                if decl.kind == "theorem":
                    theorems.append(decl.name)
                elif decl.kind == "claim":
                    claims.append(decl.name)
                elif decl.kind == "fun":
                    funs.append(decl.name)

        return {
            "ok": True,
            "elapsed": elapsed,
            "global_count": len(global_ctx.globals),
            "theorems": theorems,
            "claims": claims,
            "funs": funs,
        }
    except Exception as e:
        return {
            "ok": False,
            "elapsed": time.perf_counter() - t0,
            "error": f"{type(e).__name__}: {e}",
        }


class NatAllStrategiesTest(unittest.TestCase):

    def test_all_strategies_pass(self) -> None:
        results: dict[str, dict] = {}
        for strategy in ("greedy", "whnf", "whnfv2"):
            with self.subTest(strategy=strategy):
                r = run_nat(strategy)
                results[strategy] = r
                self.assertTrue(r["ok"], f"{strategy} failed: {r.get('error')}")

        # Cross-validate: all strategies must agree on theorem/fun lists
        ref = results["whnf"]
        for name, r in results.items():
            self.assertEqual(ref["theorems"], r["theorems"],
                             f"{name} theorem list differs from whnf")
            self.assertEqual(ref["funs"], r["funs"],
                             f"{name} fun list differs from whnf")

        # Emit comparison table (use utf-8 wrapper so checkmark survives)
        out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
        out.write("\n" + "=" * 60 + "\n")
        out.write("Nat.txt -- 3-strategy comparison\n")
        out.write("=" * 60 + "\n")
        for name, r in results.items():
            out.write(f"\n  [{name}]\n")
            out.write(f"    time:        {r['elapsed']*1000:.1f} ms\n")
            out.write(f"    globals:     {r['global_count']}\n")
            out.write(f"    theorems ({len(r['theorems'])}):\n")
            for t in r["theorems"]:
                out.write(f"      - {t}\n")
            out.write(f"    funs ({len(r['funs'])}):\n")
            for f in r["funs"]:
                out.write(f"      - {f}\n")
        out.write(f"\n  All 3 strategies pass" + " ✓\n")
        out.write("=" * 60 + "\n")
        out.flush()


if __name__ == "__main__":
    unittest.main()
