# frozen_string_literal: true

require "test_helper"

# Tests for the C template parser implementation
# This file tests parsing of all Liquid control flow tags
class TemplateParserTest < Minitest::Test
  #-----------------------------------------------------------------------------
  # If/Elsif/Else Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_simple_if
    template = Liquid::Template.parse("{% if true %}yes{% endif %}")
    assert_equal("yes", template.render!)
  end

  def test_parse_if_else
    template = Liquid::Template.parse("{% if false %}yes{% else %}no{% endif %}")
    assert_equal("no", template.render!)
  end

  def test_parse_if_elsif_else
    source = <<~LIQUID
      {% if x == 1 %}one{% elsif x == 2 %}two{% else %}other{% endif %}
    LIQUID
    template = Liquid::Template.parse(source.strip)

    assert_equal("one", template.render!({ "x" => 1 }))
    assert_equal("two", template.render!({ "x" => 2 }))
    assert_equal("other", template.render!({ "x" => 3 }))
  end

  def test_parse_if_with_and_or_operators
    template = Liquid::Template.parse("{% if a and b %}both{% endif %}")
    assert_equal("both", template.render!({ "a" => true, "b" => true }))
    assert_equal("", template.render!({ "a" => true, "b" => false }))

    template = Liquid::Template.parse("{% if a or b %}either{% endif %}")
    assert_equal("either", template.render!({ "a" => false, "b" => true }))
    assert_equal("", template.render!({ "a" => false, "b" => false }))
  end

  def test_parse_if_with_comparison_operators
    operators = {
      "==" => [1, 1, true],
      "!=" => [1, 2, true],
      "<" => [1, 2, true],
      ">" => [2, 1, true],
      "<=" => [1, 1, true],
      ">=" => [2, 1, true],
      "contains" => ["hello world", "world", true],
    }

    operators.each do |op, (a, b, expected_true)|
      template = Liquid::Template.parse("{% if a #{op} b %}yes{% endif %}")
      result = template.render!({ "a" => a, "b" => b })
      if expected_true
        assert_equal("yes", result, "Operator #{op} failed")
      else
        assert_equal("", result, "Operator #{op} failed")
      end
    end
  end

  def test_parse_nested_if
    source = <<~LIQUID
      {% if outer %}{% if inner %}both{% else %}outer_only{% endif %}{% endif %}
    LIQUID
    template = Liquid::Template.parse(source.strip)

    assert_equal("both", template.render!({ "outer" => true, "inner" => true }))
    assert_equal("outer_only", template.render!({ "outer" => true, "inner" => false }))
    assert_equal("", template.render!({ "outer" => false, "inner" => true }))
  end

  #-----------------------------------------------------------------------------
  # Unless Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_simple_unless
    template = Liquid::Template.parse("{% unless false %}yes{% endunless %}")
    assert_equal("yes", template.render!)
  end

  def test_parse_unless_else
    template = Liquid::Template.parse("{% unless true %}no{% else %}yes{% endunless %}")
    assert_equal("yes", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Case/When Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_simple_case
    source = <<~LIQUID
      {% case x %}{% when 1 %}one{% when 2 %}two{% endcase %}
    LIQUID
    template = Liquid::Template.parse(source.strip)

    assert_equal("one", template.render!({ "x" => 1 }))
    assert_equal("two", template.render!({ "x" => 2 }))
    assert_equal("", template.render!({ "x" => 3 }))
  end

  def test_parse_case_with_else
    source = <<~LIQUID
      {% case x %}{% when 1 %}one{% else %}other{% endcase %}
    LIQUID
    template = Liquid::Template.parse(source.strip)

    assert_equal("one", template.render!({ "x" => 1 }))
    assert_equal("other", template.render!({ "x" => 2 }))
  end

  def test_parse_case_with_multiple_values
    source = <<~LIQUID
      {% case x %}{% when 1, 2, 3 %}small{% when 4, 5 %}medium{% endcase %}
    LIQUID
    template = Liquid::Template.parse(source.strip)

    assert_equal("small", template.render!({ "x" => 2 }))
    assert_equal("medium", template.render!({ "x" => 4 }))
  end

  #-----------------------------------------------------------------------------
  # For Loop Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_simple_for
    template = Liquid::Template.parse("{% for i in (1..3) %}{{ i }}{% endfor %}")
    assert_equal("123", template.render!)
  end

  def test_parse_for_with_array
    template = Liquid::Template.parse("{% for item in items %}{{ item }},{% endfor %}")
    assert_equal("a,b,c,", template.render!({ "items" => %w[a b c] }))
  end

  def test_parse_for_with_limit
    template = Liquid::Template.parse("{% for i in (1..5) limit:2 %}{{ i }}{% endfor %}")
    assert_equal("12", template.render!)
  end

  def test_parse_for_with_offset
    template = Liquid::Template.parse("{% for i in (1..5) offset:2 %}{{ i }}{% endfor %}")
    assert_equal("345", template.render!)
  end

  def test_parse_for_with_reversed
    template = Liquid::Template.parse("{% for i in (1..3) reversed %}{{ i }}{% endfor %}")
    assert_equal("321", template.render!)
  end

  def test_parse_for_with_else
    template = Liquid::Template.parse("{% for item in items %}{{ item }}{% else %}empty{% endfor %}")
    assert_equal("empty", template.render!({ "items" => [] }))
  end

  def test_parse_for_forloop_variables
    source = <<~LIQUID
      {% for i in (1..3) %}{{ forloop.index }}-{{ forloop.first }}-{{ forloop.last }},{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("1-true-false,2-false-false,3-false-true,", template.render!)
  end

  def test_parse_nested_for
    source = <<~LIQUID
      {% for i in (1..2) %}{% for j in (1..2) %}({{ i }},{{ j }}){% endfor %}{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("(1,1)(1,2)(2,1)(2,2)", template.render!)
  end

  def test_parse_for_with_break
    template = Liquid::Template.parse("{% for i in (1..5) %}{% if i == 3 %}{% break %}{% endif %}{{ i }}{% endfor %}")
    assert_equal("12", template.render!)
  end

  def test_parse_for_with_continue
    template = Liquid::Template.parse("{% for i in (1..5) %}{% if i == 3 %}{% continue %}{% endif %}{{ i }}{% endfor %}")
    assert_equal("1245", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Tablerow Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_tablerow
    template = Liquid::Template.parse("{% tablerow i in (1..3) %}{{ i }}{% endtablerow %}")
    output = template.render!
    assert_includes(output, "<tr")
    assert_includes(output, "<td")
    assert_includes(output, "1")
    assert_includes(output, "2")
    assert_includes(output, "3")
  end

  def test_parse_tablerow_with_cols
    template = Liquid::Template.parse("{% tablerow i in (1..4) cols:2 %}{{ i }}{% endtablerow %}")
    output = template.render!
    # Should create 2 rows with 2 columns each
    assert_equal(2, output.scan("<tr").count)
  end

  #-----------------------------------------------------------------------------
  # Variable Assignment Tags
  #-----------------------------------------------------------------------------

  def test_parse_assign
    template = Liquid::Template.parse("{% assign x = 42 %}{{ x }}")
    assert_equal("42", template.render!)
  end

  def test_parse_assign_with_filter
    template = Liquid::Template.parse("{% assign x = 'hello' | upcase %}{{ x }}")
    assert_equal("HELLO", template.render!)
  end

  def test_parse_capture
    template = Liquid::Template.parse("{% capture x %}hello world{% endcapture %}{{ x }}")
    assert_equal("hello world", template.render!)
  end

  def test_parse_capture_with_nested_tags
    source = <<~LIQUID
      {% capture x %}{% for i in (1..3) %}{{ i }}{% endfor %}{% endcapture %}{{ x }}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("123", template.render!)
  end

  def test_parse_increment
    template = Liquid::Template.parse("{% increment x %}{% increment x %}{% increment x %}")
    assert_equal("012", template.render!)
  end

  def test_parse_decrement
    template = Liquid::Template.parse("{% decrement x %}{% decrement x %}{% decrement x %}")
    assert_equal("-1-2-3", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Comment and Raw Tags
  #-----------------------------------------------------------------------------

  def test_parse_comment
    template = Liquid::Template.parse("before{% comment %}hidden{% endcomment %}after")
    assert_equal("beforeafter", template.render!)
  end

  def test_parse_raw
    template = Liquid::Template.parse("{% raw %}{{ not_evaluated }}{% endraw %}")
    assert_equal("{{ not_evaluated }}", template.render!)
  end

  def test_parse_raw_with_liquid_like_content
    template = Liquid::Template.parse("{% raw %}{% if true %}{% endif %}{% endraw %}")
    assert_equal("{% if true %}{% endif %}", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Echo Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_echo
    template = Liquid::Template.parse("{% echo 'hello' %}")
    assert_equal("hello", template.render!)
  end

  def test_parse_echo_with_filter
    template = Liquid::Template.parse("{% echo 'hello' | upcase %}")
    assert_equal("HELLO", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Cycle Tag Tests
  #-----------------------------------------------------------------------------

  def test_parse_cycle
    template = Liquid::Template.parse("{% for i in (1..4) %}{% cycle 'a', 'b' %}{% endfor %}")
    assert_equal("abab", template.render!)
  end

  def test_parse_cycle_with_group
    source = <<~LIQUID
      {% for i in (1..2) %}{% cycle 'g1': 'a', 'b' %}{% cycle 'g2': 'x', 'y' %}{% endfor %}
    LIQUID
    template = Liquid::Template.parse(source.strip)
    assert_equal("axby", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Whitespace Control Tests
  #-----------------------------------------------------------------------------

  def test_parse_whitespace_trim_left
    template = Liquid::Template.parse("  {{- 'x' }}")
    assert_equal("x", template.render!)
  end

  def test_parse_whitespace_trim_right
    template = Liquid::Template.parse("{{ 'x' -}}  ")
    assert_equal("x", template.render!)
  end

  def test_parse_whitespace_trim_both
    template = Liquid::Template.parse("  {{- 'x' -}}  ")
    assert_equal("x", template.render!)
  end

  def test_parse_tag_whitespace_trim
    template = Liquid::Template.parse("  {%- if true -%}  x  {%- endif -%}  ")
    assert_equal("x", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Complex Nested Templates
  #-----------------------------------------------------------------------------

  def test_parse_complex_nested_template
    source = <<~LIQUID
      {% for category in categories %}
        {{ category.name }}:
        {% for product in category.products %}
          {% if product.in_stock %}
            - {{ product.name }} (${{ product.price }})
          {% endif %}
        {% endfor %}
      {% endfor %}
    LIQUID
    template = Liquid::Template.parse(source)

    data = {
      "categories" => [
        {
          "name" => "Electronics",
          "products" => [
            { "name" => "Phone", "price" => 999, "in_stock" => true },
            { "name" => "Laptop", "price" => 1999, "in_stock" => false },
          ],
        },
        {
          "name" => "Books",
          "products" => [
            { "name" => "Ruby Guide", "price" => 49, "in_stock" => true },
          ],
        },
      ],
    }

    output = template.render!(data)
    assert_includes(output, "Electronics")
    assert_includes(output, "Phone")
    assert_includes(output, "$999")
    refute_includes(output, "Laptop") # out of stock
    assert_includes(output, "Ruby Guide")
  end

  def test_parse_deeply_nested_if
    source = <<~LIQUID
      {% if a %}
        {% if b %}
          {% if c %}
            {% if d %}
              deep
            {% endif %}
          {% endif %}
        {% endif %}
      {% endif %}
    LIQUID
    template = Liquid::Template.parse(source)

    assert_includes(template.render!({ "a" => true, "b" => true, "c" => true, "d" => true }), "deep")
    refute_includes(template.render!({ "a" => true, "b" => true, "c" => true, "d" => false }), "deep")
  end

  def test_parse_mixed_control_flow
    source = <<~LIQUID
      {% case type %}
        {% when 'list' %}
          {% for item in items %}
            {% if item.visible %}{{ item.name }}{% endif %}
          {% endfor %}
        {% when 'count' %}
          {{ items | size }}
        {% else %}
          unknown
      {% endcase %}
    LIQUID
    template = Liquid::Template.parse(source)

    list_data = {
      "type" => "list",
      "items" => [
        { "name" => "A", "visible" => true },
        { "name" => "B", "visible" => false },
        { "name" => "C", "visible" => true },
      ],
    }
    output = template.render!(list_data)
    assert_includes(output, "A")
    assert_includes(output, "C")
    refute_includes(output, "B")

    count_data = { "type" => "count", "items" => [1, 2, 3] }
    assert_includes(template.render!(count_data), "3")

    assert_includes(template.render!({ "type" => "other" }), "unknown")
  end
end
