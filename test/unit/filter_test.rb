# frozen_string_literal: true

require "test_helper"

class FilterTest < MiniTest::Test
  def test_filter_split
    source = "{{ 'ooooooooox' | split: 'o' }}"
    template = Liquid::Template.parse(source)
    assert_equal "x", template.render

    assert_equal(0, template.errors.size)
  end

  def test_filter_split_missing_argument_error
    source = "{{ 'ooooooooox' | split }}"
    template = Liquid::Template.parse(source)

    assert_equal('Liquid error: wrong number of arguments (given 1, expected 2)', template.render)
    assert_equal(1, template.errors.size)
    assert_equal(Liquid::ArgumentError, template.errors.first.class)
  end

  def test_filter_split_wrong_arguement_error
    source = "{{ 'ooooooooox' | split: 'o' foo: '1'}}"
    template = Liquid::Template.parse(source)

    assert_equal('Liquid error: wrong number of arguments (given 3, expected 2)', template.render)
    assert_equal(1, template.errors.size)
    assert_equal(Liquid::ArgumentError, template.errors.first.class)
  end
end
