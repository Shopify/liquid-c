# frozen_string_literal: true

require "test_helper"

# Tests for new VM opcodes added for the C template parser
# Based on parser_design.md opcode specifications
class TemplateParserVmOpcodesTest < Minitest::Test
  #-----------------------------------------------------------------------------
  # Comparison Opcodes (OP_CMP_*)
  #-----------------------------------------------------------------------------

  def test_comparison_equal
    template = Liquid::Template.parse("{% if a == b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 1, "b" => 1 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 2 }))
  end

  def test_comparison_not_equal
    template = Liquid::Template.parse("{% if a != b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 1, "b" => 2 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 1 }))
  end

  def test_comparison_less_than
    template = Liquid::Template.parse("{% if a < b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 1, "b" => 2 }))
    assert_equal("", template.render!({ "a" => 2, "b" => 1 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 1 }))
  end

  def test_comparison_greater_than
    template = Liquid::Template.parse("{% if a > b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 2, "b" => 1 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 2 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 1 }))
  end

  def test_comparison_less_than_or_equal
    template = Liquid::Template.parse("{% if a <= b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 1, "b" => 2 }))
    assert_equal("yes", template.render!({ "a" => 1, "b" => 1 }))
    assert_equal("", template.render!({ "a" => 2, "b" => 1 }))
  end

  def test_comparison_greater_than_or_equal
    template = Liquid::Template.parse("{% if a >= b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => 2, "b" => 1 }))
    assert_equal("yes", template.render!({ "a" => 1, "b" => 1 }))
    assert_equal("", template.render!({ "a" => 1, "b" => 2 }))
  end

  def test_comparison_contains
    template = Liquid::Template.parse("{% if a contains b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => "hello world", "b" => "world" }))
    assert_equal("", template.render!({ "a" => "hello world", "b" => "foo" }))
    # Array contains
    assert_equal("yes", template.render!({ "a" => [1, 2, 3], "b" => 2 }))
    assert_equal("", template.render!({ "a" => [1, 2, 3], "b" => 4 }))
  end

  #-----------------------------------------------------------------------------
  # Liquid Truthiness (OP_TRUTHY, OP_NOT)
  #-----------------------------------------------------------------------------

  def test_liquid_truthiness_nil_is_falsy
    template = Liquid::Template.parse("{% if x %}yes{% else %}no{% endif %}")
    assert_equal("no", template.render!({ "x" => nil }))
  end

  def test_liquid_truthiness_false_is_falsy
    template = Liquid::Template.parse("{% if x %}yes{% else %}no{% endif %}")
    assert_equal("no", template.render!({ "x" => false }))
  end

  def test_liquid_truthiness_zero_is_truthy
    template = Liquid::Template.parse("{% if x %}yes{% else %}no{% endif %}")
    assert_equal("yes", template.render!({ "x" => 0 }))
  end

  def test_liquid_truthiness_empty_string_is_truthy
    template = Liquid::Template.parse("{% if x %}yes{% else %}no{% endif %}")
    assert_equal("yes", template.render!({ "x" => "" }))
  end

  def test_liquid_truthiness_empty_array_is_truthy
    template = Liquid::Template.parse("{% if x %}yes{% else %}no{% endif %}")
    assert_equal("yes", template.render!({ "x" => [] }))
  end

  #-----------------------------------------------------------------------------
  # Logical Operators (and/or)
  #-----------------------------------------------------------------------------

  def test_logical_and
    template = Liquid::Template.parse("{% if a and b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => true, "b" => true }))
    assert_equal("", template.render!({ "a" => true, "b" => false }))
    assert_equal("", template.render!({ "a" => false, "b" => true }))
    assert_equal("", template.render!({ "a" => false, "b" => false }))
  end

  def test_logical_or
    template = Liquid::Template.parse("{% if a or b %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => true, "b" => true }))
    assert_equal("yes", template.render!({ "a" => true, "b" => false }))
    assert_equal("yes", template.render!({ "a" => false, "b" => true }))
    assert_equal("", template.render!({ "a" => false, "b" => false }))
  end

  def test_logical_chained_and
    template = Liquid::Template.parse("{% if a and b and c %}yes{% endif %}")
    assert_equal("yes", template.render!({ "a" => true, "b" => true, "c" => true }))
    assert_equal("", template.render!({ "a" => true, "b" => true, "c" => false }))
  end

  def test_logical_chained_or
    template = Liquid::Template.parse("{% if a or b or c %}yes{% endif %}")
    assert_equal("", template.render!({ "a" => false, "b" => false, "c" => false }))
    assert_equal("yes", template.render!({ "a" => false, "b" => false, "c" => true }))
  end

  def test_logical_mixed_and_or
    # Liquid evaluates left to right, no precedence
    # a or b and c => a or (b and c) in terms of short-circuit evaluation
    template = Liquid::Template.parse("{% if a or b and c %}yes{% endif %}")
    # If 'a' is true, short-circuits to true
    assert_equal("yes", template.render!({ "a" => true, "b" => false, "c" => true }))
    assert_equal("yes", template.render!({ "a" => true, "b" => false, "c" => false }))
    # If 'a' is false, evaluates 'b and c'
    assert_equal("yes", template.render!({ "a" => false, "b" => true, "c" => true }))
    assert_equal("", template.render!({ "a" => false, "b" => true, "c" => false }))
  end

  #-----------------------------------------------------------------------------
  # Jump Opcodes (OP_JUMP, OP_JUMP_IF_FALSE, OP_JUMP_IF_TRUE)
  #-----------------------------------------------------------------------------

  def test_jump_forward_in_if
    # Tests that the parser generates correct forward jumps
    template = Liquid::Template.parse("{% if false %}skip{% endif %}after")
    assert_equal("after", template.render!)
  end

  def test_jump_to_else
    template = Liquid::Template.parse("{% if false %}then{% else %}else{% endif %}after")
    assert_equal("elseafter", template.render!)
  end

  def test_jump_in_elsif_chain
    source = "{% if x == 1 %}one{% elsif x == 2 %}two{% elsif x == 3 %}three{% else %}other{% endif %}"
    template = Liquid::Template.parse(source)
    assert_equal("one", template.render!({ "x" => 1 }))
    assert_equal("two", template.render!({ "x" => 2 }))
    assert_equal("three", template.render!({ "x" => 3 }))
    assert_equal("other", template.render!({ "x" => 4 }))
  end

  def test_wide_jump_for_large_template
    # Generate a template large enough to require wide jumps (>256 bytes)
    large_content = "x" * 300
    template = Liquid::Template.parse("{% if false %}#{large_content}{% endif %}after")
    assert_equal("after", template.render!)
  end

  #-----------------------------------------------------------------------------
  # For Loop Opcodes (OP_FOR_INIT, OP_FOR_NEXT, OP_FOR_CLEANUP)
  #-----------------------------------------------------------------------------

  def test_for_loop_basic_iteration
    template = Liquid::Template.parse("{% for i in items %}{{ i }}{% endfor %}")
    assert_equal("abc", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_with_range
    template = Liquid::Template.parse("{% for i in (1..3) %}{{ i }}{% endfor %}")
    assert_equal("123", template.render!)
  end

  def test_for_loop_empty_collection
    template = Liquid::Template.parse("{% for i in items %}{{ i }}{% else %}empty{% endfor %}")
    assert_equal("empty", template.render!({ "items" => [] }))
  end

  def test_for_loop_with_limit
    template = Liquid::Template.parse("{% for i in items limit:2 %}{{ i }}{% endfor %}")
    assert_equal("ab", template.render!({ "items" => %w[a b c d] }))
  end

  def test_for_loop_with_offset
    template = Liquid::Template.parse("{% for i in items offset:2 %}{{ i }}{% endfor %}")
    assert_equal("cd", template.render!({ "items" => %w[a b c d] }))
  end

  def test_for_loop_with_limit_and_offset
    template = Liquid::Template.parse("{% for i in items limit:2 offset:1 %}{{ i }}{% endfor %}")
    assert_equal("bc", template.render!({ "items" => %w[a b c d e] }))
  end

  def test_for_loop_reversed
    template = Liquid::Template.parse("{% for i in items reversed %}{{ i }}{% endfor %}")
    assert_equal("cba", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_index
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.index }}{% endfor %}")
    assert_equal("123", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_index0
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.index0 }}{% endfor %}")
    assert_equal("012", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_rindex
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.rindex }}{% endfor %}")
    assert_equal("321", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_rindex0
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.rindex0 }}{% endfor %}")
    assert_equal("210", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_first
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.first }}{% endfor %}")
    assert_equal("truefalsefalse", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_last
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.last }}{% endfor %}")
    assert_equal("falsefalsetrue", template.render!({ "items" => %w[a b c] }))
  end

  def test_for_loop_forloop_length
    template = Liquid::Template.parse("{% for i in items %}{{ forloop.length }}{% endfor %}")
    assert_equal("333", template.render!({ "items" => %w[a b c] }))
  end

  #-----------------------------------------------------------------------------
  # Break and Continue Opcodes (OP_BREAK, OP_CONTINUE)
  #-----------------------------------------------------------------------------

  def test_break_in_loop
    template = Liquid::Template.parse("{% for i in (1..5) %}{% if i == 3 %}{% break %}{% endif %}{{ i }}{% endfor %}")
    assert_equal("12", template.render!)
  end

  def test_continue_in_loop
    template = Liquid::Template.parse("{% for i in (1..5) %}{% if i == 3 %}{% continue %}{% endif %}{{ i }}{% endfor %}")
    assert_equal("1245", template.render!)
  end

  def test_break_in_nested_loop
    source = <<~LIQUID
      {% for i in (1..3) %}{% for j in (1..3) %}{% if j == 2 %}{% break %}{% endif %}{{ j }}{% endfor %}|{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("1|1|1|", template.render!)
  end

  def test_continue_in_nested_loop
    source = <<~LIQUID
      {% for i in (1..2) %}{% for j in (1..3) %}{% if j == 2 %}{% continue %}{% endif %}{{ j }}{% endfor %}|{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("13|13|", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Variable Opcodes (OP_ASSIGN, OP_CAPTURE_START, OP_CAPTURE_END)
  #-----------------------------------------------------------------------------

  def test_assign_simple
    template = Liquid::Template.parse("{% assign x = 42 %}{{ x }}")
    assert_equal("42", template.render!)
  end

  def test_assign_with_expression
    template = Liquid::Template.parse("{% assign x = a | plus: b %}{{ x }}")
    assert_equal("5", template.render!({ "a" => 2, "b" => 3 }))
  end

  def test_assign_overwrites
    template = Liquid::Template.parse("{% assign x = 1 %}{% assign x = 2 %}{{ x }}")
    assert_equal("2", template.render!)
  end

  def test_capture_simple
    template = Liquid::Template.parse("{% capture x %}hello{% endcapture %}{{ x }}")
    assert_equal("hello", template.render!)
  end

  def test_capture_with_expressions
    template = Liquid::Template.parse("{% capture x %}{{ a }} and {{ b }}{% endcapture %}{{ x }}")
    assert_equal("1 and 2", template.render!({ "a" => 1, "b" => 2 }))
  end

  def test_capture_with_control_flow
    source = "{% capture x %}{% for i in (1..3) %}{{ i }}{% endfor %}{% endcapture %}{{ x }}"
    template = Liquid::Template.parse(source)
    assert_equal("123", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Counter Opcodes (OP_INCREMENT, OP_DECREMENT)
  #-----------------------------------------------------------------------------

  def test_increment_basic
    template = Liquid::Template.parse("{% increment x %}{% increment x %}{% increment x %}")
    assert_equal("012", template.render!)
  end

  def test_decrement_basic
    template = Liquid::Template.parse("{% decrement x %}{% decrement x %}{% decrement x %}")
    assert_equal("-1-2-3", template.render!)
  end

  def test_increment_independent_of_assign
    template = Liquid::Template.parse("{% assign x = 10 %}{% increment x %}{{ x }}")
    assert_equal("010", template.render!)
  end

  def test_decrement_independent_of_assign
    template = Liquid::Template.parse("{% assign x = 10 %}{% decrement x %}{{ x }}")
    assert_equal("-110", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Cycle Opcode (OP_CYCLE)
  #-----------------------------------------------------------------------------

  def test_cycle_basic
    template = Liquid::Template.parse("{% for i in (1..5) %}{% cycle 'a', 'b', 'c' %}{% endfor %}")
    assert_equal("abcab", template.render!)
  end

  def test_cycle_with_group
    source = <<~LIQUID
      {% for i in (1..4) %}{% cycle 'g1': 'a', 'b' %}{% cycle 'g2': 'x', 'y' %}{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    # Each named group cycles independently through its values
    assert_equal("axbyaxby", template.render!)
  end

  def test_cycle_persists_across_loops
    source = <<~LIQUID
      {% for i in (1..2) %}{% cycle 'a', 'b', 'c' %}{% endfor %}|{% for i in (1..2) %}{% cycle 'a', 'b', 'c' %}{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("ab|ca", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Case Opcode (OP_CASE_EQ)
  #-----------------------------------------------------------------------------

  def test_case_basic
    source = "{% case x %}{% when 1 %}one{% when 2 %}two{% else %}other{% endcase %}"
    template = Liquid::Template.parse(source)
    assert_equal("one", template.render!({ "x" => 1 }))
    assert_equal("two", template.render!({ "x" => 2 }))
    assert_equal("other", template.render!({ "x" => 3 }))
  end

  def test_case_with_strings
    source = '{% case x %}{% when "a" %}A{% when "b" %}B{% endcase %}'
    template = Liquid::Template.parse(source)
    assert_equal("A", template.render!({ "x" => "a" }))
    assert_equal("B", template.render!({ "x" => "b" }))
  end

  def test_case_with_multiple_when_values
    source = "{% case x %}{% when 1, 2, 3 %}small{% when 4, 5 %}medium{% endcase %}"
    template = Liquid::Template.parse(source)
    assert_equal("small", template.render!({ "x" => 1 }))
    assert_equal("small", template.render!({ "x" => 2 }))
    assert_equal("medium", template.render!({ "x" => 4 }))
    assert_equal("", template.render!({ "x" => 6 }))
  end

  #-----------------------------------------------------------------------------
  # Tablerow Opcodes (OP_TABLEROW_*)
  #-----------------------------------------------------------------------------

  def test_tablerow_basic
    template = Liquid::Template.parse("{% tablerow i in (1..3) %}{{ i }}{% endtablerow %}")
    output = template.render!
    assert_includes(output, "<tr")
    assert_includes(output, "<td")
    assert_includes(output, "</td>")
    assert_includes(output, "</tr>")
  end

  def test_tablerow_with_cols
    template = Liquid::Template.parse("{% tablerow i in (1..6) cols:3 %}{{ i }}{% endtablerow %}")
    output = template.render!
    # Should have 2 rows
    assert_equal(2, output.scan("<tr").count)
  end

  def test_tablerow_with_limit_and_offset
    template = Liquid::Template.parse("{% tablerow i in items limit:2 offset:1 %}{{ i }}{% endtablerow %}")
    output = template.render!({ "items" => %w[a b c d e] })
    assert_includes(output, "b")
    assert_includes(output, "c")
    refute_includes(output, ">a<")
    refute_includes(output, ">d<")
  end
end
