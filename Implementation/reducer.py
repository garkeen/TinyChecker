from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

from .core import CApp, CGlobal, CLam, CPi, CTerm, CType, CVar, mk_apps, unfold_app
from .core_ops import instantiate, instantiate_env
from .errors import ReducerError
from .runtime import ConstructorInfo, EliminatorInfo, TcGlobalContext

ConvStrategy = Literal["greedy", "whnf", "whnfv2"]


@dataclass
class Reducer:
    global_ctx: TcGlobalContext
    conv_strategy: ConvStrategy = "whnf"
    max_steps: int = 200_000

    def step_head(self, term: CTerm) -> CTerm | None:
        return self.try_step_head(term, allow_delta=True)

    def whnf(self, term: CTerm) -> CTerm:
        return self._whnf(term, allow_delta=True)

    def whnf_no_delta(self, term: CTerm) -> CTerm:
        return self._whnf(term, allow_delta=False)

    def _whnf(self, term: CTerm, *, allow_delta: bool) -> CTerm:
        current = term
        steps = 0
        while True:
            steps += 1
            if steps > self.max_steps:
                raise ReducerError("WHNF exceeded step limit")
            stepped = self.try_step_head(current, allow_delta=allow_delta)
            if stepped is not None:
                current = stepped
                continue
            if isinstance(current, CApp):
                fn_whnf = self._whnf(current.func, allow_delta=allow_delta)
                if fn_whnf != current.func:
                    current = CApp(fn_whnf, current.arg)
                    continue
            return current

    def try_step_head(self, term: CTerm, *, allow_delta: bool) -> CTerm | None:
        head, args = unfold_app(term)
        beta = self.try_beta(head, args)
        if beta is not None:
            return beta
        if isinstance(head, CGlobal):
            iota = self.try_iota(head.name, args)
            if iota is not None:
                return iota
            delta = self.try_delta(head.name, args, allow_delta=allow_delta)
            if delta is not None:
                return delta
        return None

    def try_beta(self, head: CTerm, args: list[CTerm]) -> CTerm | None:
        if isinstance(head, CLam) and args:
            return mk_apps(instantiate(head.body, args[0]), args[1:])
        return None

    def nf(self, term: CTerm) -> CTerm:
        wh = self.whnf(term)
        if isinstance(wh, CType | CVar | CGlobal):
            return wh
        if isinstance(wh, CPi):
            return CPi(wh.name, self.nf(wh.domain), self.nf(wh.codomain))
        if isinstance(wh, CLam):
            return CLam(wh.name, self.nf(wh.param_type), self.nf(wh.body))
        if isinstance(wh, CApp):
            rebuilt = CApp(self.nf(wh.func), self.nf(wh.arg))
            stepped = self.step_head(rebuilt)
            if stepped is None:
                return rebuilt
            return self.nf(stepped)
        raise ReducerError(f"unknown term in NF: {wh}")

    def is_def_eq(self, lhs: CTerm, rhs: CTerm) -> bool:
        if self.conv_strategy == "greedy":
            return self._is_def_eq_greedy(lhs, rhs, steps=0)
        if self.conv_strategy == "whnfv2":
            return self._is_def_eq_whnfv2(lhs, rhs)
        return self._is_def_eq_whnf(lhs, rhs)

    def try_delta(self, head_name: str, args: list[CTerm], *, allow_delta: bool) -> CTerm | None:
        entry = self.global_ctx.globals.get(head_name)
        if entry is None:
            raise ReducerError(f"unknown global {head_name}")
        if allow_delta and entry.value is not None:
            return mk_apps(entry.value, args)
        return None

    def _is_def_eq_greedy(self, lhs: CTerm, rhs: CTerm, *, steps: int) -> bool:
        if steps > self.max_steps:
            raise ReducerError("greedy conversion exceeded step limit")
        if lhs == rhs:
            return True

        # -- case 1: CType vs CType --
        if isinstance(lhs, CType) and isinstance(rhs, CType):
            return True

        # -- case 15: CVar vs CVar --
        if isinstance(lhs, CVar) and isinstance(rhs, CVar):
            return lhs.index == rhs.index

        # -- case 15: CGlobal vs CGlobal --
        if isinstance(lhs, CGlobal) and isinstance(rhs, CGlobal):
            if lhs.name == rhs.name:
                return True
            lv = self._global_value(lhs.name)
            rv = self._global_value(rhs.name)
            if lv is not None and rv is not None:
                return self._is_def_eq_greedy(lv, rv, steps=steps + 1)
            if lv is not None:
                return self._is_def_eq_greedy(lv, rhs, steps=steps + 1)
            if rv is not None:
                return self._is_def_eq_greedy(lhs, rv, steps=steps + 1)
            return False

        # -- case 13: CPi vs CPi --
        if isinstance(lhs, CPi) and isinstance(rhs, CPi):
            return self._is_def_eq_greedy(lhs.domain, rhs.domain, steps=steps + 1) and self._is_def_eq_greedy(
                lhs.codomain,
                rhs.codomain,
                steps=steps + 1,
            )

        # -- case 10: CLam vs CLam --
        if isinstance(lhs, CLam) and isinstance(rhs, CLam):
            return self._is_def_eq_greedy(lhs.param_type, rhs.param_type, steps=steps + 1) and self._is_def_eq_greedy(
                lhs.body,
                rhs.body,
                steps=steps + 1,
            )

        # -- case 6: CApp vs CApp --
        if isinstance(lhs, CApp) and isinstance(rhs, CApp):
            if self._is_def_eq_greedy(lhs.func, rhs.func, steps=steps + 1) and self._is_def_eq_greedy(
                lhs.arg,
                rhs.arg,
                steps=steps + 1,
            ):
                return True
            # sub-comparison failed — reduce both sides, at least one must step
            ls = self.step_head(lhs)
            rs = self.step_head(rhs)
            if ls is None and rs is None:
                return False
            return self._is_def_eq_greedy(
                ls if ls is not None else lhs,
                rs if rs is not None else rhs,
                steps=steps + 1,
            )

        # === cross-type cases ===

        # -- case 2: CType vs V --
        if isinstance(lhs, CType) and isinstance(rhs, (CVar, CGlobal)):
            rv = self._global_value(rhs.name) if isinstance(rhs, CGlobal) else None
            if rv is not None:
                return self._is_def_eq_greedy(lhs, rv, steps=steps + 1)
            return False
        if isinstance(lhs, (CVar, CGlobal)) and isinstance(rhs, CType):
            lv = self._global_value(lhs.name) if isinstance(lhs, CGlobal) else None
            if lv is not None:
                return self._is_def_eq_greedy(lv, rhs, steps=steps + 1)
            return False

        # -- case 3: CType vs CApp --
        if isinstance(lhs, CType) and isinstance(rhs, CApp):
            rs = self.step_head(rhs)
            if rs is None:
                return False
            return self._is_def_eq_greedy(lhs, rs, steps=steps + 1)
        if isinstance(lhs, CApp) and isinstance(rhs, CType):
            ls = self.step_head(lhs)
            if ls is None:
                return False
            return self._is_def_eq_greedy(ls, rhs, steps=steps + 1)

        # -- cases 4, 5: CType vs CLam / CType vs CPi --
        if isinstance(lhs, CType) and isinstance(rhs, (CLam, CPi)):
            return False
        if isinstance(lhs, (CLam, CPi)) and isinstance(rhs, CType):
            return False

        # -- case 9: CApp vs V --
        if isinstance(lhs, CApp) and isinstance(rhs, (CVar, CGlobal)):
            rv = self._global_value(rhs.name) if isinstance(rhs, CGlobal) else None
            if rv is not None:
                return self._is_def_eq_greedy(lhs, rv, steps=steps + 1)
            ls = self.step_head(lhs)
            if ls is not None:
                return self._is_def_eq_greedy(ls, rhs, steps=steps + 1)
            return False
        if isinstance(lhs, (CVar, CGlobal)) and isinstance(rhs, CApp):
            lv = self._global_value(lhs.name) if isinstance(lhs, CGlobal) else None
            if lv is not None:
                return self._is_def_eq_greedy(lv, rhs, steps=steps + 1)
            rs = self.step_head(rhs)
            if rs is not None:
                return self._is_def_eq_greedy(lhs, rs, steps=steps + 1)
            return False

        # -- cases 7, 8: CApp vs CLam / CApp vs CPi --
        if isinstance(lhs, CApp) and isinstance(rhs, (CLam, CPi)):
            ls = self.step_head(lhs)
            if ls is None:
                return False
            return self._is_def_eq_greedy(ls, rhs, steps=steps + 1)
        if isinstance(lhs, (CLam, CPi)) and isinstance(rhs, CApp):
            rs = self.step_head(rhs)
            if rs is None:
                return False
            return self._is_def_eq_greedy(lhs, rs, steps=steps + 1)

        # -- cases 12, 14: CLam vs V / CPi vs V --
        if isinstance(lhs, (CLam, CPi)) and isinstance(rhs, (CVar, CGlobal)):
            rv = self._global_value(rhs.name) if isinstance(rhs, CGlobal) else None
            if rv is not None:
                return self._is_def_eq_greedy(lhs, rv, steps=steps + 1)
            return False
        if isinstance(lhs, (CVar, CGlobal)) and isinstance(rhs, (CLam, CPi)):
            lv = self._global_value(lhs.name) if isinstance(lhs, CGlobal) else None
            if lv is not None:
                return self._is_def_eq_greedy(lv, rhs, steps=steps + 1)
            return False

        # -- case 15: CVar vs CGlobal (mixed V vs V) --
        if isinstance(lhs, CVar) and isinstance(rhs, CGlobal):
            rv = self._global_value(rhs.name)
            if rv is not None:
                return self._is_def_eq_greedy(lhs, rv, steps=steps + 1)
            return False
        if isinstance(lhs, CGlobal) and isinstance(rhs, CVar):
            lv = self._global_value(lhs.name)
            if lv is not None:
                return self._is_def_eq_greedy(lv, rhs, steps=steps + 1)
            return False

        # -- case 11: CLam vs CPi, and any other unmatched combinations --
        return False

    def _global_value(self, name: str) -> CTerm | None:
        entry = self.global_ctx.globals.get(name)
        if entry is not None and entry.value is not None:
            return entry.value
        return None

    def _is_def_eq_whnf(self, lhs: CTerm, rhs: CTerm) -> bool:
        # Doc rule 1: quick syntax check on original terms
        if lhs == rhs:
            return True
        # Doc rules 2-8: WHNF (with Delta) then structural compare
        return self._whnf_compare(lhs, rhs, allow_delta=True)

    def _is_def_eq_whnfv2(self, lhs: CTerm, rhs: CTerm) -> bool:
        # Doc: greedy WHNF — outer loop picks the lowest-height global to Delta-expand,
        # inner comparison uses pure Beta/Iota WHNF (no Delta).
        current_lhs = lhs
        current_rhs = rhs
        steps = 0
        while True:
            steps += 1
            if steps > self.max_steps:
                raise ReducerError("whnfv2 conversion exceeded step limit")
            # Doc step 1: quick syntax check
            if current_lhs == current_rhs:
                return True
            # Doc step 2: Beta + Iota WHNF structural compare (no Delta)
            if self._whnf_compare(current_lhs, current_rhs, allow_delta=False):
                return True
            # Doc step 3: pick lowest-height expandable global
            choice = self._pick_delta_name(current_lhs, current_rhs)
            # Doc step 5: no expandable → fail
            if choice is None:
                return False
            # Doc step 4: Delta-expand the chosen name on both sides, loop
            next_lhs = self._expand_named_global(current_lhs, choice)
            next_rhs = self._expand_named_global(current_rhs, choice)
            if next_lhs == current_lhs and next_rhs == current_rhs:
                return False
            current_lhs = next_lhs
            current_rhs = next_rhs

    def _whnf_compare(self, lhs: CTerm, rhs: CTerm, *, allow_delta: bool) -> bool:
        """WHNF both sides, then structural comparison.

        Uses unfold_app for CApp so that the stuck head (guaranteed CVar or
        valueless CGlobal in WHNF) is compared by identity without re-whnf-ing.
        """
        a = self.whnf(lhs) if allow_delta else self.whnf_no_delta(lhs)
        b = self.whnf(rhs) if allow_delta else self.whnf_no_delta(rhs)
        if a == b:
            return True
        if type(a) is not type(b):
            return False
        # Doc rule 2: CType
        if isinstance(a, CType):
            return True
        # Doc rule 3: CVar
        if isinstance(a, CVar):
            return a.index == b.index
        # Doc rule 4: CGlobal
        if isinstance(a, CGlobal):
            return a.name == b.name
        # Doc rule 5: CPi — recurse on domain and codomain
        if isinstance(a, CPi):
            return self._whnf_compare(a.domain, b.domain, allow_delta=allow_delta) and self._whnf_compare(
                a.codomain, b.codomain, allow_delta=allow_delta,
            )
        # Doc rule 6: CLam — recurse on param_type and body
        if isinstance(a, CLam):
            return self._whnf_compare(a.param_type, b.param_type, allow_delta=allow_delta) and self._whnf_compare(
                a.body, b.body, allow_delta=allow_delta,
            )
        # Doc rule 7: CApp — unfold to application chain to avoid redundant
        # WHNF on the stuck head; only arguments need WHNF comparison.
        if isinstance(a, CApp):
            ahead, a_args = unfold_app(a)
            bhead, b_args = unfold_app(b)
            if len(a_args) != len(b_args):
                return False
            return self._whnf_compare(ahead, bhead, allow_delta=allow_delta) and all(
                self._whnf_compare(aa, ba, allow_delta=allow_delta)
                for aa, ba in zip(a_args, b_args)
            )
        # Doc rule 8: otherwise fail
        return False

    def _pick_delta_name(self, lhs: CTerm, rhs: CTerm) -> str | None:
        candidates = self._collect_expandable_globals(lhs) | self._collect_expandable_globals(rhs)
        if not candidates:
            return None
        cache: dict[str, int] = {}
        return min(candidates, key=lambda name: (self._global_height(name, cache, set()), name))

    def _collect_expandable_globals(self, term: CTerm) -> set[str]:
        if isinstance(term, CGlobal):
            entry = self.global_ctx.globals.get(term.name)
            if entry is not None and entry.value is not None:
                return {term.name}
            return set()
        if isinstance(term, CType | CVar):
            return set()
        if isinstance(term, CPi):
            return self._collect_expandable_globals(term.domain) | self._collect_expandable_globals(term.codomain)
        if isinstance(term, CLam):
            return self._collect_expandable_globals(term.param_type) | self._collect_expandable_globals(term.body)
        if isinstance(term, CApp):
            return self._collect_expandable_globals(term.func) | self._collect_expandable_globals(term.arg)
        raise ReducerError(f"unknown term in global collection: {term}")

    def _global_height(self, name: str, cache: dict[str, int], visiting: set[str]) -> int:
        if name in cache:
            return cache[name]
        if name in visiting:
            return self.max_steps
        entry = self.global_ctx.globals.get(name)
        if entry is None or entry.value is None:
            cache[name] = 0
            return 0
        visiting.add(name)
        refs = self._collect_expandable_globals(entry.value)
        if not refs:
            height = 1
        else:
            height = 1 + max(self._global_height(ref, cache, visiting) for ref in refs)
        visiting.remove(name)
        cache[name] = height
        return height

    def _expand_named_global(self, term: CTerm, target_name: str) -> CTerm:
        if isinstance(term, CGlobal):
            if term.name != target_name:
                return term
            entry = self.global_ctx.globals.get(target_name)
            if entry is None or entry.value is None:
                return term
            return entry.value
        if isinstance(term, CType | CVar):
            return term
        if isinstance(term, CPi):
            domain = self._expand_named_global(term.domain, target_name)
            codomain = self._expand_named_global(term.codomain, target_name)
            if domain == term.domain and codomain == term.codomain:
                return term
            return CPi(term.name, domain, codomain)
        if isinstance(term, CLam):
            param_type = self._expand_named_global(term.param_type, target_name)
            body = self._expand_named_global(term.body, target_name)
            if param_type == term.param_type and body == term.body:
                return term
            return CLam(term.name, param_type, body)
        if isinstance(term, CApp):
            func = self._expand_named_global(term.func, target_name)
            arg = self._expand_named_global(term.arg, target_name)
            if func == term.func and arg == term.arg:
                return term
            return CApp(func, arg)
        raise ReducerError(f"unknown term in delta expansion: {term}")

    def try_iota(self, head_name: str, args: list[CTerm]) -> CTerm | None:
        elim = self.global_ctx.eliminators.get(head_name)
        if elim is None:
            return None
        return self.try_iota_eliminator(elim, args)

    def try_iota_eliminator(self, elim: EliminatorInfo, args: list[CTerm]) -> CTerm | None:
        ind = self.global_ctx.inductives[elim.owner]
        branch_count = len(elim.branch_order)
        arity = ind.param_count + 1 + branch_count + ind.index_count + 1
        if len(args) < arity:
            return None
        params = args[: ind.param_count]
        motive = args[ind.param_count]
        branches = args[ind.param_count + 1 : ind.param_count + 1 + branch_count]
        indices = args[ind.param_count + 1 + branch_count : ind.param_count + 1 + branch_count + ind.index_count]
        scrutinee = args[arity - 1]
        extra = args[arity:]
        scrutinee_whnf = self.whnf(scrutinee)
        ctor_view = self.view_constructor(scrutinee_whnf, elim.owner)
        if ctor_view is None:
            return None
        ctor_info, ctor_args = ctor_view
        field_args = ctor_args[ctor_info.param_count :]
        branch = branches[elim.branch_order.index(ctor_info.name)]
        ihs = self.build_ih_terms(elim, motive, branches, params, indices, field_args, ctor_info)
        reduced = mk_apps(branch, field_args + ihs)
        if extra:
            reduced = mk_apps(reduced, extra)
        return reduced

    def build_ih_terms(
        self,
        elim: EliminatorInfo,
        motive: CTerm,
        branches: list[CTerm],
        params: list[CTerm],
        indices: list[CTerm],
        field_args: list[CTerm],
        ctor_info: ConstructorInfo,
    ) -> list[CTerm]:
        ihs: list[CTerm] = []
        for rec in ctor_info.recursive_fields:
            prefix_args = params + field_args[: rec.field_position]
            prefix_env = list(reversed(prefix_args))
            target_args = [
                instantiate_env(arg, prefix_env)
                for arg in rec.recursive_target_args[ctor_info.param_count :]
            ]
            field_value = field_args[rec.field_position]
            if not rec.ho_telescope:
                ihs.append(
                    mk_apps(
                        CGlobal(elim.name),
                        params + [motive] + branches + target_args + [field_value],
                    )
                )
                continue
            ho_names = [name for name, _ in rec.ho_telescope]
            ho_vars = [CVar(len(rec.ho_telescope) - 1 - i, name) for i, name in enumerate(ho_names)]
            field_applied = mk_apps(field_value, ho_vars)
            body = mk_apps(
                CGlobal(elim.name),
                params + [motive] + branches + target_args + [field_applied],
            )
            lam_body = body
            for name, ty in reversed(rec.ho_telescope):
                lam_body = CLam(name, instantiate_env(ty, prefix_env), lam_body)
            ihs.append(lam_body)
        return ihs

    def view_constructor(self, term: CTerm, inductive_name: str) -> tuple[ConstructorInfo, list[CTerm]] | None:
        head, args = unfold_app(term)
        if not isinstance(head, CGlobal):
            return None
        info = self.global_ctx.constructors.get(head.name)
        if info is None or info.owner != inductive_name:
            return None
        return info, args
