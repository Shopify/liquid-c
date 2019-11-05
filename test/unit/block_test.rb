require 'test_helper'

class BlockTest < MiniTest::Test
  def test_no_allocation_of_trimmed_strings
    template = Liquid::Template.parse("{{ -}}     {{- }}")
    assert_equal 2, template.root.nodelist.size

    template = Liquid::Template.parse("{{ -}} foo {{- }}")
    assert_equal 3, template.root.nodelist.size
  end
end
