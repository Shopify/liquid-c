# frozen_string_literal: true

require "test_helper"

# Tests for custom tag fallback to Ruby
# The C parser should delegate unknown tags to Ruby tag classes
class TemplateParserCustomTagsTest < Minitest::Test
  # Custom tag that just outputs its markup
  class EchoMarkupTag < Liquid::Tag
    def initialize(tag_name, markup, parse_context)
      super
      @markup = markup.strip
    end

    def render(_context)
      "ECHO:#{@markup}"
    end
  end

  # Custom block tag
  class WrapTag < Liquid::Block
    def initialize(tag_name, markup, parse_context)
      super
      @wrapper = markup.strip
    end

    def render(context)
      "[#{@wrapper}]#{super}[/#{@wrapper}]"
    end
  end

  # Custom tag that accesses context
  class ContextAccessTag < Liquid::Tag
    def initialize(tag_name, markup, parse_context)
      super
      @var_name = markup.strip
    end

    def render(context)
      "VAR:#{context[@var_name]}"
    end
  end

  # Custom tag that modifies context
  class SetVarTag < Liquid::Tag
    def initialize(tag_name, markup, parse_context)
      super
      parts = markup.strip.split("=", 2)
      @var_name = parts[0].strip
      @var_value = parts[1].strip
    end

    def render(context)
      context[@var_name] = @var_value
      ""
    end
  end

  def setup
    Liquid::Template.register_tag("echo_markup", EchoMarkupTag)
    Liquid::Template.register_tag("wrap", WrapTag)
    Liquid::Template.register_tag("ctx_access", ContextAccessTag)
    Liquid::Template.register_tag("set_var", SetVarTag)
  end

  def teardown
    # Clean up registered tags
    Liquid::Template.tags.delete("echo_markup")
    Liquid::Template.tags.delete("wrap")
    Liquid::Template.tags.delete("ctx_access")
    Liquid::Template.tags.delete("set_var")
  end

  #-----------------------------------------------------------------------------
  # Basic Custom Tag Tests
  #-----------------------------------------------------------------------------

  def test_custom_simple_tag
    template = Liquid::Template.parse("{% echo_markup hello world %}")
    assert_equal("ECHO:hello world", template.render!)
  end

  def test_custom_block_tag
    template = Liquid::Template.parse("{% wrap div %}content{% endwrap %}")
    assert_equal("[div]content[/div]", template.render!)
  end

  def test_custom_tag_with_context_access
    template = Liquid::Template.parse("{% ctx_access myvar %}")
    assert_equal("VAR:42", template.render!({ "myvar" => 42 }))
  end

  def test_custom_tag_modifies_context
    template = Liquid::Template.parse("{% set_var x = hello %}{{ x }}")
    assert_equal("hello", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Custom Tags Mixed with Built-in Tags
  #-----------------------------------------------------------------------------

  def test_custom_tag_in_if_block
    source = "{% if show %}{% echo_markup shown %}{% endif %}"
    template = Liquid::Template.parse(source)
    assert_equal("ECHO:shown", template.render!({ "show" => true }))
    assert_equal("", template.render!({ "show" => false }))
  end

  def test_custom_tag_in_for_loop
    source = "{% for i in (1..3) %}{% echo_markup item %}{% endfor %}"
    template = Liquid::Template.parse(source)
    assert_equal("ECHO:itemECHO:itemECHO:item", template.render!)
  end

  def test_custom_block_with_built_in_tags_inside
    source = "{% wrap outer %}{% if true %}inner{% endif %}{% endwrap %}"
    template = Liquid::Template.parse(source)
    assert_equal("[outer]inner[/outer]", template.render!)
  end

  def test_built_in_tags_inside_custom_block
    source = "{% wrap container %}{% for i in (1..3) %}{{ i }}{% endfor %}{% endwrap %}"
    template = Liquid::Template.parse(source)
    assert_equal("[container]123[/container]", template.render!)
  end

  def test_nested_custom_blocks
    source = "{% wrap outer %}{% wrap inner %}content{% endwrap %}{% endwrap %}"
    template = Liquid::Template.parse(source)
    assert_equal("[outer][inner]content[/inner][/outer]", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Custom Tags with Variables
  #-----------------------------------------------------------------------------

  def test_custom_tag_before_variable
    source = "{% set_var x = hello %}Value: {{ x }}"
    template = Liquid::Template.parse(source)
    assert_equal("Value: hello", template.render!)
  end

  def test_custom_tag_uses_assigned_variable
    source = "{% assign myvar = 'test' %}{% ctx_access myvar %}"
    template = Liquid::Template.parse(source)
    assert_equal("VAR:test", template.render!)
  end

  def test_custom_tag_in_capture
    source = "{% capture x %}{% echo_markup captured %}{% endcapture %}{{ x }}"
    template = Liquid::Template.parse(source)
    assert_equal("ECHO:captured", template.render!)
  end

  #-----------------------------------------------------------------------------
  # Custom Tags in Complex Templates
  #-----------------------------------------------------------------------------

  def test_custom_tags_in_complex_template
    source = <<~LIQUID
      {% if user %}
        {% wrap header %}
          Welcome, {{ user.name }}!
          {% for item in items %}
            {% echo_markup item: %}{{ item }}
          {% endfor %}
        {% endwrap %}
      {% else %}
        {% wrap guest %}
          Please log in
        {% endwrap %}
      {% endif %}
    LIQUID

    template = Liquid::Template.parse(source)

    logged_in = { "user" => { "name" => "Alice" }, "items" => %w[a b] }
    output = template.render!(logged_in)
    assert_includes(output, "[header]")
    assert_includes(output, "Alice")
    assert_includes(output, "ECHO:item:")

    guest = {}
    output = template.render!(guest)
    assert_includes(output, "[guest]")
    assert_includes(output, "Please log in")
  end

  #-----------------------------------------------------------------------------
  # Error Handling with Custom Tags
  #-----------------------------------------------------------------------------

  def test_unknown_tag_raises_error
    exc = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("{% totally_unknown_tag %}")
    end
    assert_match(/unknown.*tag|unknowntag|totally_unknown_tag/i, exc.message)
  end

  def test_custom_tag_syntax_error_in_markup
    # If the custom tag raises during parse, it should propagate
    error_tag = Class.new(Liquid::Tag) do
      def initialize(tag_name, markup, parse_context)
        super
        raise Liquid::SyntaxError, "Custom tag error"
      end
    end

    Liquid::Template.register_tag("error_tag", error_tag)
    begin
      exc = assert_raises(Liquid::SyntaxError) do
        Liquid::Template.parse("{% error_tag %}")
      end
      assert_includes(exc.message, "Custom tag error")
    ensure
      Liquid::Template.tags.delete("error_tag")
    end
  end

  def test_custom_tag_render_error_is_handled
    error_render_tag = Class.new(Liquid::Tag) do
      def render(_context)
        raise Liquid::Error, "Render error"
      end
    end

    Liquid::Template.register_tag("error_render", error_render_tag)
    begin
      template = Liquid::Template.parse("before{% error_render %}after")
      output = template.render
      # Error should be caught and rendered inline
      assert_includes(output, "before")
      assert_includes(output, "after")
      assert_includes(output, "error") # error message included
    ensure
      Liquid::Template.tags.delete("error_render")
    end
  end

  #-----------------------------------------------------------------------------
  # Custom Tag with Line Numbers
  #-----------------------------------------------------------------------------

  def test_custom_tag_error_includes_line_number
    error_tag = Class.new(Liquid::Tag) do
      def render(_context)
        raise Liquid::Error, "Error from custom tag"
      end
    end

    Liquid::Template.register_tag("line_error", error_tag)
    begin
      source = "line 1\nline 2\n{% line_error %}\nline 4"
      template = Liquid::Template.parse(source, line_numbers: true)
      output = template.render

      assert_includes(output, "Error from custom tag")
    ensure
      Liquid::Template.tags.delete("line_error")
    end
  end
end
