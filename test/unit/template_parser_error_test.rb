# frozen_string_literal: true

require "test_helper"

# Error handling tests for the C template parser
class TemplateParserErrorTest < Minitest::Test
  #-----------------------------------------------------------------------------
  # Unclosed Tag Errors
  #-----------------------------------------------------------------------------

  def test_unclosed_if_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% if true %}missing endif")
    end
    assert_match(/if.*never closed|tag.*not.*closed|endif/i, exc.message)
  end

  def test_unclosed_unless_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% unless false %}missing endunless")
    end
    assert_match(/unless.*never closed|tag.*not.*closed|endunless/i, exc.message)
  end

  def test_unclosed_for_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% for i in items %}missing endfor")
    end
    assert_match(/for.*never closed|tag.*not.*closed|endfor/i, exc.message)
  end

  def test_unclosed_case_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% case x %}{% when 1 %}one")
    end
    assert_match(/case.*never closed|tag.*not.*closed|endcase/i, exc.message)
  end

  def test_unclosed_capture_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% capture x %}missing endcapture")
    end
    assert_match(/capture.*never closed|tag.*not.*closed|endcapture/i, exc.message)
  end

  def test_unclosed_comment_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% comment %}missing endcomment")
    end
    assert_match(/comment.*never closed|tag.*not.*closed|endcomment/i, exc.message)
  end

  def test_unclosed_raw_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% raw %}missing endraw")
    end
    assert_match(/raw.*never closed|tag.*not.*closed|endraw/i, exc.message)
  end

  def test_unclosed_tablerow_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% tablerow i in items %}missing endtablerow")
    end
    assert_match(/tablerow.*never closed|tag.*not.*closed|endtablerow/i, exc.message)
  end

  #-----------------------------------------------------------------------------
  # Invalid Tag Syntax Errors
  #-----------------------------------------------------------------------------

  def test_if_without_condition
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% if %}yes{% endif %}")
    end
    assert(exc.message)
  end

  def test_for_without_variable
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% for in items %}{{ i }}{% endfor %}")
    end
    assert(exc.message)
  end

  def test_for_without_in_keyword
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% for i items %}{{ i }}{% endfor %}")
    end
    assert(exc.message)
  end

  def test_for_without_collection
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% for i in %}{{ i }}{% endfor %}")
    end
    assert(exc.message)
  end

  def test_case_without_variable
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% case %}{% when 1 %}one{% endcase %}")
    end
    assert(exc.message)
  end

  def test_when_without_value
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% case x %}{% when %}one{% endcase %}")
    end
    assert(exc.message)
  end

  def test_assign_without_variable
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% assign = 42 %}")
    end
    assert(exc.message)
  end

  def test_assign_without_value
    # `{% assign x = %}` silently assigns nil/empty to x
    # This is valid Liquid syntax (assigns empty)
    template = Liquid::Template.parse("{% assign x = %}{{ x }}")
    assert_equal("", template.render!)
  end

  def test_capture_without_variable
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% capture %}content{% endcapture %}")
    end
    assert(exc.message)
  end

  def test_cycle_without_values
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% cycle %}")
    end
    assert(exc.message)
  end

  #-----------------------------------------------------------------------------
  # Mismatched Tag Errors
  #-----------------------------------------------------------------------------

  def test_endif_without_if
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% endif %}")
    end
    assert_match(/endif|unexpected|unknown/i, exc.message)
  end

  def test_endfor_without_for
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% endfor %}")
    end
    assert_match(/endfor|unexpected|unknown/i, exc.message)
  end

  def test_else_without_if
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% else %}")
    end
    assert_match(/else|unexpected|unknown/i, exc.message)
  end

  def test_elsif_without_if
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% elsif true %}")
    end
    assert_match(/elsif|unexpected|unknown/i, exc.message)
  end

  def test_when_without_case
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% when 1 %}")
    end
    assert_match(/when|unexpected|unknown/i, exc.message)
  end

  def test_break_outside_loop
    # break/continue outside of for loop is silently ignored in Liquid
    # They render as empty, no error is raised
    template = Liquid::Template.parse("{% break %}")
    assert_equal("", template.render!)
  end

  def test_continue_outside_loop
    # break/continue outside of for loop is silently ignored in Liquid
    template = Liquid::Template.parse("{% continue %}")
    assert_equal("", template.render!)
  end

  def test_mismatched_end_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% if true %}{% endfor %}")
    end
    assert(exc.message)
  end

  #-----------------------------------------------------------------------------
  # Invalid Expression Errors
  #-----------------------------------------------------------------------------

  def test_invalid_variable_expression
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{{ @ }}", error_mode: :strict)
    end
    assert(exc.message)
  end

  def test_invalid_comparison_operator
    # Unknown operators render as error at runtime in lax mode
    template = Liquid::Template.parse("{% if a === b %}yes{% endif %}")
    output = template.render({ "a" => 1, "b" => 1 })
    assert_includes(output, "error")
  end

  def test_unclosed_string_in_expression_strict
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse('{% if x == "unclosed %}yes{% endif %}', error_mode: :strict)
    end
    assert(exc.message)
  end

  def test_unclosed_bracket_in_lookup_strict
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{{ array[0 }}", error_mode: :strict)
    end
    assert(exc.message)
  end

  def test_invalid_range_syntax_renders_empty
    # Triple dots are not valid range syntax, but in lax mode it may not error
    # In strict mode it should error
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% for i in (1...3) %}{{ i }}{% endfor %}", error_mode: :strict)
    end
    assert(exc.message)
  end

  #-----------------------------------------------------------------------------
  # Unknown Tag Errors
  #-----------------------------------------------------------------------------

  def test_unknown_tag
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% unknowntag %}")
    end
    assert_match(/unknown.*tag|unknowntag/i, exc.message)
  end

  #-----------------------------------------------------------------------------
  # Line Number Reporting
  #-----------------------------------------------------------------------------

  def test_error_includes_line_number
    source = <<~LIQUID
      line 1
      line 2
      {% if true
      line 4
    LIQUID

    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse(source, line_numbers: true)
    end
    # Error should reference the line where the error occurred
    assert(exc.line_number || exc.message =~ /line/i)
  end

  def test_error_in_nested_template
    source = <<~LIQUID
      {% for i in items %}
        {% if condition
      {% endfor %}
    LIQUID

    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse(source, line_numbers: true)
    end
    assert(exc.message)
  end

  #-----------------------------------------------------------------------------
  # Edge Cases
  #-----------------------------------------------------------------------------

  def test_empty_template
    template = Liquid::Template.parse("")
    assert_equal("", template.render!)
  end

  def test_only_whitespace
    template = Liquid::Template.parse("   \n\t\n   ")
    assert_equal("   \n\t\n   ", template.render!)
  end

  def test_only_raw_text
    template = Liquid::Template.parse("Hello, World!")
    assert_equal("Hello, World!", template.render!)
  end

  def test_deeply_nested_unclosed_tags
    source = <<~LIQUID
      {% if a %}
        {% for i in items %}
          {% if b %}
            {% case x %}
              {% when 1 %}
                missing many endtags
    LIQUID

    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse(source)
    end
    assert(exc.message)
  end

  def test_multiple_errors_in_template
    # Parser should report the first error encountered
    source = "{% if %}{% for %}{% unknowntag %}"

    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse(source)
    end
    assert(exc.message)
  end
end
