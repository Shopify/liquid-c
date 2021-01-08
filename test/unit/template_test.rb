# frozen_string_literal: true

require 'test_helper'

class TemplateTest < MiniTest::Test
  def test_serialize
    assert_equal('hello world', dump_load_eval('hello world'))
    assert_equal('hello world', dump_load_eval('{% assign greeting = "hello" %}{{ greeting }} world'))
    assert_equal('hello world', dump_load_eval('{% raw %}hello {% endraw %}world'))
    assert_equal('hello world',
      dump_load_eval('{% if test %}goodbye {% else %}hello {% endif %}world', 'test' => false))
    assert_equal('hello world', dump_load_eval('{% if true %}hello {% endif %}{% if true %}world{% endif %}'))
    assert_equal('123', dump_load_eval('{% for i in (1..10) %}{{i}}{% if i == 3 %}{% break %}{% endif %}{% endfor %}'))
  end

  def test_serialize_with_line_numbers
    template = <<-LIQUID
      Hello,

      {{ errors.standard_error }} will raise a standard error.
    LIQUID

    expected = <<-TEXT
      Hello,

      Liquid error (line 3): standard error will raise a standard error.
    TEXT

    error_drop_klass = Class.new(Liquid::Drop) do
      def standard_error
        raise Liquid::StandardError, 'standard error'
      end
    end

    assert_equal(expected, dump_load_eval(template, { 'errors' => error_drop_klass.new }, { line_numbers: true }))
  end

  private

  def dump_load_eval(source, assigns = {}, options = {})
    serialize = Liquid::Template.parse(source, options).dump
    Liquid::Template.load(serialize).render(assigns)
  end
end
