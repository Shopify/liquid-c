# Comprehensive Spec Failure Analysis

**Date:** 2026-01-27
**Current Status:** 501/618 (81%) - DOWN from 511/618 (83%)
**Target:** 95%+ (586+ specs)

## CRITICAL: Regressions Detected

The recent changes introduced regressions in break/continue handling within for loops.

### Regression: break/continue in for loops (NEW FAILURES)

**Specs that WERE passing but NOW fail:**
- `for_break` - `{% break %}` no longer stops loop iteration
- `for_continue` - `{% continue %}` no longer skips iteration
- `break_propagates_through_if` - break inside if doesn't propagate to for loop
- `continue_propagates_through_if` - continue inside if doesn't propagate
- And several more break/continue propagation specs

**Example:**
```liquid
{% for i in (1..5) %}{% if i == 3 %}{% break %}{% endif %}{{ i }}{% endfor %}
```
- Expected: `12`
- Actual: `12345` (break is ignored)

**Priority: CRITICAL** - This is a major regression affecting core loop functionality.

---

## Failure Categories

### 1. Break/Continue Handling (13 failures) - REGRESSION
**Priority:** CRITICAL
**Failures:** 13 specs

| Spec | Issue |
|------|-------|
| `for_break` | break doesn't stop loop |
| `for_continue` | continue doesn't skip iteration |
| `break_propagates_through_if` | break in if block ignored |
| `break_propagates_through_nested_if` | nested if break ignored |
| `continue_propagates_through_if` | continue in if block ignored |
| `break_propagates_through_case` | break in case ignored |
| `break_propagates_through_unless` | break in unless ignored |
| `break_affects_innermost_loop_only` | wrong loop affected |
| `continue_affects_innermost_loop_only` | wrong loop affected |
| `break_in_if_outside_loop` | should error but doesn't |
| `tablerow_break` | break in tablerow |
| `tablerow_continue` | continue in tablerow |
| `break_contained_in_render` | (also needs render) |

**Likely Fix Location:** `ext/liquid_c/liquid_vm.c` or `ext/liquid_c/block.c` - the interrupt handling code

**Example Fix Needed:**
```liquid
{% for i in (1..5) %}{% if i == 3 %}{% break %}{% endif %}{{ i }}{% endfor %}
```
Must output `12`, not `12345`.

---

### 2. Empty/Blank Keyword Comparison (5 failures) - EXISTING BUG
**Priority:** HIGH
**Failures:** 5 specs

| Spec | Issue |
|------|-------|
| `empty_array_is_empty` | `[] == empty` returns false |
| `empty_hash_is_empty` | `{} == empty` returns false |
| `unless_empty_guard_blocks_output` | empty check fails in unless |
| `empty_comparison_array` | another empty array check |

**Likely Fix Location:** `ext/liquid_c/expression.c` or comparison evaluation code

**Example:**
```liquid
{% if items == empty %}empty{% else %}not{% endif %}
```
With `items = []`, should output `empty`, outputs `not`.

---

### 3. Nil Contains Check (1 failure) - EXISTING BUG
**Priority:** MEDIUM
**Failures:** 1 spec

| Spec | Issue |
|------|-------|
| `nil_in_contains_check` | `contains nil` behaves incorrectly |

**Example:**
```liquid
{% if items contains nil %}yes{% else %}no{% endif %}
```
With `items = [1, nil, 3]`, should output `no`, outputs `yes`.

**Note:** In Liquid, `contains` should not match nil elements.

---

### 4. Render/Include Tags (66 failures) - ENVIRONMENT ISSUE
**Priority:** LOW (not a code bug)
**Failures:** 66 specs

All specs testing `{% render %}` and `{% include %}` tags fail because the test environment doesn't have a filesystem configured.

**Example error:** `Liquid error: This liquid context does not allow includes.`

**Not actionable** - requires test setup changes, not code fixes.

---

### 5. Date Filter Now/Today (9 failures) - ENVIRONMENT ISSUE
**Priority:** LOW (not a code bug)
**Failures:** 9 specs

Date specs fail because time isn't frozen in test runner.

| Spec | Issue |
|------|-------|
| `date_now_keyword` | `now` outputs current time |
| `date_today_keyword` | `today` outputs current date |
| etc. | |

**Not actionable** - the filters work correctly, just can't match frozen time expectations.

---

### 6. Inline Error Format (10 failures) - COSMETIC
**Priority:** LOW
**Failures:** 10 specs

Error message formatting differs from spec expectations. Errors are still reported, just in a different format.

**Not critical** - cosmetic difference in error output.

---

### 7. Cycle in Render/Include (4 failures) - ENVIRONMENT ISSUE
**Priority:** LOW
**Failures:** 4 specs

Depends on render/include working.

---

### 8. Recursion Handling (3 failures) - ENVIRONMENT ISSUE
**Priority:** LOW
**Failures:** 3 specs

Depends on render/include working.

---

## Summary by Priority

| Priority | Category | Failures | Actionable? |
|----------|----------|----------|-------------|
| CRITICAL | Break/Continue regression | 13 | YES - FIX IMMEDIATELY |
| HIGH | Empty keyword comparison | 5 | YES |
| MEDIUM | Nil contains check | 1 | YES |
| LOW | Render/Include (env) | 66 | NO - test setup |
| LOW | Date filters (env) | 9 | NO - test setup |
| LOW | Inline errors (cosmetic) | 10 | Optional |
| LOW | Cycle in partials (env) | 4 | NO - test setup |
| LOW | Recursion (env) | 3 | NO - test setup |

**Actionable failures:** 19 specs
**Environment/cosmetic issues:** 92 specs

## Path to 95%+ Conformance

1. **FIX REGRESSION:** Break/continue handling (13 specs) → +13 specs
2. **FIX:** Empty keyword comparison (5 specs) → +5 specs
3. **FIX:** Nil contains check (1 spec) → +1 spec

**After fixes:** 501 + 19 = 520/618 (84%)

To reach 95% (586 specs), we would also need to:
- Configure filesystem for render/include tests (66 specs)
- Fix inline error formatting (10 specs)

**Realistic target with code fixes only:** 520/618 (84%)
**Target with test environment setup:** 586/618 (95%)
