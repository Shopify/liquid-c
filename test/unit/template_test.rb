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

  private

  def dump_load_eval(source, assigns = {})
    serialize = Liquid::Template.parse(source).dump
    Liquid::Template.load(serialize).render!(assigns)
  end
end
