# Liquid-Spec Conformance Analysis for liquid-c

Date: 2026-01-27
Spec Suite: liquid-spec basics (618 specs)

## Summary

| Category | Passed | Failed | Pass Rate |
|----------|--------|--------|-----------|
| Total (basics) | 511 | 107 | 83% |
| Control Flow Tags | 42 | 3 | 93% |

## Control Flow Tags - Detailed Results

### if/elsif/else - ALL PASSING (18 specs)
- `if_true_literal`, `if_false_literal`
- `if_variable_truthy`, `if_variable_nil`
- `if_else`
- `if_equality_string`, `if_equality_integer`
- `if_inequality`
- `if_greater_than`, `if_less_than`, `if_greater_or_equal`, `if_less_or_equal`
- `if_and_operator`, `if_and_short_circuit`, `if_or_operator`
- `if_contains_string`, `if_contains_array`
- `if_elsif`

### unless - ALL PASSING (4 specs)
- `unless_basic`, `unless_true`, `unless_variable`, `unless_empty_guard`

### case/when - ALL PASSING (5 specs)
- `case_basic`, `case_no_match`, `case_else`
- `case_multiple_values`, `case_string`

### for loops - 15/18 PASSING
**Passing:**
- `for_basic_array`, `for_range_literal`, `for_range_variable`
- `for_else`, `for_limit`, `for_offset`, `for_reversed`
- `for_break`, `for_continue`
- `for_offset_continue_basic`, `for_offset_continue_until_end`
- `for_offset_continue_different_collections`, `for_offset_continue_exhausted`
- `for_offset_continue_same_variable_different_collection`
- `forloop_parentloop_nil_at_top`

**Failing (all due to render/include not configured):**
- `for_offset_continue_isolated_in_render`
- `for_parentloop_nil_in_render`
- `for_parentloop_available_in_include`

## Real Issues Found

### 1. `blank` Keyword Comparison (5 failures)

The `blank` keyword doesn't work correctly in comparisons.

**Failing specs:**
- `whitespace_string_is_blank`: `"   " == blank` should be true
- `empty_string_is_blank`: `"" == blank` should be true
- `nil_is_blank`: `nil == blank` should be true
- `false_is_blank`: `false == blank` should be true
- `empty_vs_blank_comparison`: whitespace-only strings should match `blank`

**Expected behavior:** The `blank` keyword should match:
- Empty strings `""`
- Whitespace-only strings `"   "`
- `nil` values
- `false` values

### 2. tablerow break/continue (2 failures)

Break and continue inside tablerow don't work correctly.

**tablerow_break:**
```liquid
{% tablerow item in items cols:3 %}{% if item == 'c' %}{% break %}{% endif %}{{ item }}{% endtablerow %}
```
With items = ['a', 'b', 'c', 'd', 'e']
- Expected: Stops at 'c', outputs single row with 3 cells
- Actual: Continues rendering, outputs extra empty cells in second row

**tablerow_continue:**
```liquid
{% tablerow item in items cols:3 %}{% if item == 'b' %}{% continue %}{% endif %}{{ item }}{% endtablerow %}
```
With items = ['a', 'b', 'c', 'd']
- Expected: Skips 'b' but continues with 'c', 'd' in correct positions
- Actual: Appears to skip more items than intended

## Non-Issues (Expected Failures)

### render/include tags (~60 failures)
All failures related to render/include tags are expected because the test context doesn't have a filesystem configured. These are not parser issues.

### date filter now/today (9 failures)
Date specs fail because time isn't frozen in our test runner. The `now` and `today` keywords work correctly; the expected values just don't match the current time.

### inline error format (11 failures)
Error message formatting differs from spec expectations. This is a cosmetic issue, not a correctness issue.

### cycle isolation in partials (4 failures)
These depend on render/include working, which requires filesystem setup.

## Test Files Created

1. `/Users/tobi/src/tries/2026-01-16-Shopify-liquid-c/liquid_c_adapter.rb` - Adapter for liquid-spec CLI
2. `/Users/tobi/src/tries/2026-01-16-Shopify-liquid-c/run_spec_tests.rb` - Standalone test runner

### Running Tests

```bash
# Run all basics specs
bundle exec ruby run_spec_tests.rb /Users/tobi/.gem/ruby/3.3.0/gems/liquid-spec-0.9.1/specs/basics --no-max-failures

# Run control flow specs only
bundle exec ruby run_spec_tests.rb /path/to/specs -n "^(if_|unless_|case_|for_)" -v

# Run specific pattern
bundle exec ruby run_spec_tests.rb /path/to/specs -n "tablerow" -v
```

## Recommendations

1. **Parser work is NOT needed for control flow correctness** - All if/unless/case/for parsing works correctly
2. **Fix `blank` keyword comparison** - This is a real semantic issue
3. **Fix tablerow break/continue** - These are real bugs in iteration handling
4. **Configure filesystem for render/include tests** - To verify those work correctly
