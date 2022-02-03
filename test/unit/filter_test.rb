# frozen_string_literal: true

require "test_helper"

class FilterTest < MiniTest::Test
  def test_filter_split
    source = "{{ 'ooooooooox' | split: 'o' }}"
    template = Liquid::Template.parse(source)
    puts template.render
  end
end
