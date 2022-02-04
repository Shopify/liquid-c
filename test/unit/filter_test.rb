# frozen_string_literal: true

require "test_helper"

class FilterTest < MiniTest::Test
  def test_filter_split
    source = "{{ 'ooooooooox' | split: 'o' }}"
    template = Liquid::Template.parse(source)
    assert_equal "x", template.render
  end

  def test_filter_split_missing_argument_error
    source = "{{ 'ooooooooox' | split }}"
    template = Liquid::Template.parse(source)

    exc = assert_raises(Liquid::ArgumentError) { template.render }
    assert_equal("Liquid error: wrong number of arguments (given 0, expected 1)", exc.message)
  end

  def test_filter_split_wrong_arguement_error
    source = "{{ 'ooooooooox' | split: 'o' foo: '1'}}"
    template = Liquid::Template.parse(source)

    exc = assert_raises(Liquid::ArgumentError) { template.render }
    assert_equal("Liquid error: wrong number of arguments (given 2, expected 1)", exc.message)
  end
end
