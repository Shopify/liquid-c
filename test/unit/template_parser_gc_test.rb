# frozen_string_literal: true

require "test_helper"

# Memory safety tests for the C template parser
# These tests run with GC.stress = true to catch memory management bugs
class TemplateParserGCTest < Minitest::Test
  def setup
    skip "GC stress tests disabled; set LIQUID_C_GC_STRESS=1 to enable" unless ENV["LIQUID_C_GC_STRESS"] == "1"
  end

  #-----------------------------------------------------------------------------
  # Basic Parsing Under GC Stress
  #-----------------------------------------------------------------------------

  def test_parse_if_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% if true %}yes{% else %}no{% endif %}")
      template.render!
    end
    assert_equal("yes", result)
  end

  def test_parse_for_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% for i in (1..5) %}{{ i }}{% endfor %}")
      template.render!
    end
    assert_equal("12345", result)
  end

  def test_parse_case_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% case x %}{% when 1 %}one{% when 2 %}two{% endcase %}")
      template.render!({ "x" => 2 })
    end
    assert_equal("two", result)
  end

  def test_parse_nested_tags_under_gc_stress
    source = <<~LIQUID
      {% for i in (1..3) %}
        {% if i == 2 %}
          {% case i %}
            {% when 2 %}found{% endcase %}
        {% endif %}
      {% endfor %}
    LIQUID
    result = gc_stress do
      template = Liquid::Template.parse(source)
      template.render!
    end
    assert_includes(result, "found")
  end

  #-----------------------------------------------------------------------------
  # Variable Assignment Under GC Stress
  #-----------------------------------------------------------------------------

  def test_assign_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% assign x = 'hello' | upcase %}{{ x }}")
      template.render!
    end
    assert_equal("HELLO", result)
  end

  def test_capture_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% capture x %}{% for i in (1..3) %}{{ i }}{% endfor %}{% endcapture %}{{ x }}")
      template.render!
    end
    assert_equal("123", result)
  end

  #-----------------------------------------------------------------------------
  # Complex Templates Under GC Stress
  #-----------------------------------------------------------------------------

  def test_complex_template_under_gc_stress
    source = <<~LIQUID
      {% assign items = "a,b,c" | split: "," %}
      {% for item in items %}
        {% if forloop.first %}First: {% endif %}
        {{ item | upcase }}
        {% unless forloop.last %}, {% endunless %}
      {% endfor %}
    LIQUID

    result = gc_stress do
      template = Liquid::Template.parse(source)
      template.render!
    end
    assert_includes(result, "First:")
    assert_includes(result, "A")
    assert_includes(result, "B")
    assert_includes(result, "C")
  end

  def test_many_iterations_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% for i in (1..100) %}{{ i }}{% endfor %}")
      template.render!
    end
    assert_includes(result, "1")
    assert_includes(result, "50")
    assert_includes(result, "100")
  end

  #-----------------------------------------------------------------------------
  # Error Handling Under GC Stress
  #-----------------------------------------------------------------------------

  def test_syntax_error_under_gc_stress
    gc_stress do
      assert_raises(Liquid::SyntaxError) do
        Liquid::Template.parse("{% if true %}no endif")
      end
    end
  end

  def test_render_error_under_gc_stress
    gc_stress do
      template = Liquid::Template.parse("{{ x.missing }}")
      context = Liquid::Context.new({ "x" => {} })
      context.strict_variables = true

      assert_raises(Liquid::UndefinedVariable) do
        template.render!(context)
      end
    end
  end

  #-----------------------------------------------------------------------------
  # Repeated Parsing Under GC Stress
  #-----------------------------------------------------------------------------

  def test_repeated_parse_under_gc_stress
    gc_stress do
      10.times do |i|
        template = Liquid::Template.parse("{% if x == #{i} %}match{% endif %}")
        template.render!({ "x" => i })
      end
    end
  end

  def test_parse_many_templates_under_gc_stress
    templates = gc_stress do
      (1..20).map do |i|
        Liquid::Template.parse("template {{ #{i} }}")
      end
    end

    gc_stress do
      templates.each_with_index do |template, i|
        result = template.render!
        assert_includes(result, "template")
      end
    end
  end

  #-----------------------------------------------------------------------------
  # String Handling Under GC Stress
  #-----------------------------------------------------------------------------

  def test_unicode_strings_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{% assign x = 'hello' %}{{ x }} \u{1F600} world")
      template.render!
    end
    assert_includes(result, "hello")
    assert_includes(result, "\u{1F600}")
  end

  def test_large_string_under_gc_stress
    large_text = "x" * 10_000
    result = gc_stress do
      template = Liquid::Template.parse("prefix#{large_text}suffix")
      template.render!
    end
    assert(result.start_with?("prefix"))
    assert(result.end_with?("suffix"))
  end

  #-----------------------------------------------------------------------------
  # Object Lifecycle Under GC Stress
  #-----------------------------------------------------------------------------

  def test_template_garbage_collection
    gc_stress do
      100.times do
        Liquid::Template.parse("{% for i in (1..10) %}{{ i }}{% endfor %}")
      end
      GC.start
    end
    # If we get here without crashing, the test passes
    assert(true)
  end

  def test_context_with_template_under_gc_stress
    result = gc_stress do
      template = Liquid::Template.parse("{{ user.name }} - {{ user.email }}")
      context = Liquid::Context.new({
        "user" => {
          "name" => "Alice",
          "email" => "alice@example.com",
        },
      })
      template.render!(context)
    end
    assert_includes(result, "Alice")
    assert_includes(result, "alice@example.com")
  end

  #-----------------------------------------------------------------------------
  # Helpers
  #-----------------------------------------------------------------------------

  private

  def gc_stress
    old_value = GC.stress
    GC.stress = true
    begin
      yield
    ensure
      GC.stress = old_value
    end
  end
end
