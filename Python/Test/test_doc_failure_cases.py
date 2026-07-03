from __future__ import annotations

import unittest

from Test.support import ElabError, ParseError, elaborate_program, parse_program


class DocumentFailureCaseTests(unittest.TestCase):
    def test_nonempty_ctor_block_requires_leading_bar(self) -> None:
        src = """
inductive Nat {
  zero: Nat
  | succ: Nat -> Nat
};
"""
        with self.assertRaises(ParseError):
            parse_program(src)

    def test_underscore_cannot_appear_in_expression_position(self) -> None:
        src = "var bad : Type = _;"
        with self.assertRaises(ParseError):
            parse_program(src)

    def test_prefixed_plain_constructor_reference_is_rejected(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

var bad : Nat = Nat.zero;
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_match_can_only_eliminate_inductive(self) -> None:
        src = """
sum Flag {
| leftFlag: Flag
| rightFlag: Flag
};

fun bad (x:Flag): Flag {
  match x
  in Flag
  return Flag
  with
  | leftFlag => rightFlag
  | rightFlag => leftFlag
  end
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_case_cannot_eliminate_inductive(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun bad (n:Nat): Nat {
  case n
  in Nat
  return Nat
  of
  | zero => zero
  | succ k => k
  end
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_sum_cannot_have_recursive_field(self) -> None:
        src = """
sum Bad {
| loop: Bad -> Bad
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_product_field_name_cannot_be_mk_or_elim(self) -> None:
        src = """
product Bad {
  mk: Type
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_product_field_names_must_be_unique(self) -> None:
        src = """
product Bad {
  A: Type,
  A: Type
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_match_branches_must_be_complete_and_ordered(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun bad (n:Nat): Nat {
  match n
  in Nat
  return Nat
  with
  | succ k => k
  end
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)

    def test_bind_count_must_match_family_arity(self) -> None:
        src = """
inductive Nat {
| zero: Nat
| succ: Nat -> Nat
};

fun bad (n:Nat): Nat {
  match n
  in Nat
  bind x y
  return Nat
  with
  | zero => zero
  | succ k => k
  end
};
"""
        with self.assertRaises(ElabError):
            elaborate_program(src)


if __name__ == "__main__":
    unittest.main()
