# -*- coding: utf-8 -*-
"""负测试：验证类型检查器正确拒绝逻辑错误的证明。"""
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from Codes.pipeline import run_pipeline

PASS = 0
FAIL = 0


def must_fail(name: str, source: str) -> None:
    global PASS, FAIL
    try:
        run_pipeline(source)
        print(f"  FAIL: {name}")
        FAIL += 1
    except Exception as e:
        print(f"  OK:   {name}  ({type(e).__name__})")
        PASS += 1


# 1. Eq 类型不匹配：refl 的左右两边必须相同
must_fail("Eq类型不匹配", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Eq (A:Type): A -> A -> Type { | refl: (x:A) -> Eq A x x };
claim bad: [Nat] zero==(succ zero) = refl Nat zero;
""")

# 2. 函数参数类型错误：succ 期待 Nat，传入 Bool
must_fail("参数类型错误", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Bool { | true: Bool | false: Bool };
claim bad: Nat = succ true;
""")

# 3. match 的 motive 返回类型与分支体不一致
must_fail("match motive 类型不一致", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Bool { | true: Bool | false: Bool };
fun bad (n:Nat): Nat {
  match n in Nat return Bool with
  | zero => zero
  | succ k [ih] => succ k
  end
};
""")

# 4. match 分支数量不完整
must_fail("match 分支数量不足", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
fun bad (n:Nat): Nat {
  match n in Nat return Nat with
  | zero => zero
  end
};
""")

# 5. IH 类型与使用处不匹配
must_fail("IH 类型误用", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Eq (A:Type): A -> A -> Type { | refl: (x:A) -> Eq A x x };
fun add (a:Nat) (b:Nat): Nat {
  match a in Nat return Nat with
  | zero => b
  | succ k [ih] => succ ih
  end
};
theorem bad (a:Nat) (b:Nat): [Nat] (add a b)==(add b a) {
  match a in Nat return [Nat] (add a b)==(add b a) with
  | zero => refl Nat b
  | succ k [ih] => ih
  end
};
""")

# 6. 类型族索引不匹配：scrutinee 与 in 后面的族实例矛盾
must_fail("类型族索引矛盾", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive List (A:Type): Nat -> Type {
  | nil: List A zero
  | cons: (k:Nat) -> A -> List A k -> List A (succ k)
};
fun bad (A:Type) (L:List A zero): A {
  match L in List A (succ zero) bind T idx return A with
  | nil => bad A L
  | cons k a xs => a
  end
};
""")

# 7. 函数参数数量不足
must_fail("参数数量不足", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
fun idNat (n:Nat): Nat { n };
claim bad: Nat = idNat;
""")

# 8. 重复定义全局名
must_fail("重复全局名", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Nat { | zero: Nat | succ: Nat -> Nat };
""")

# 9. 将 Type 当作函数使用
must_fail("Type 当函数用", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
claim bad: Type = Nat zero;
""")

# 10. 数据构造器返回类型不是所属类型族
must_fail("构造器目标类型错误", """
inductive Nat { | zero: Nat | succ: Nat -> Nat };
inductive Bool { | true: Nat | false: Bool };
""")

print(f"\n负测试: {PASS} 通过 / {PASS + FAIL} 总计")
if FAIL:
    print(f"  {FAIL} 个未通过!")
