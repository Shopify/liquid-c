require 'test_helper'

class BlockTest < MiniTest::Test
  def test_no_allocation_of_trimmed_strings
    template = Liquid::Template.parse("{{ -}}     {{- }}")
    assert_equal 2, template.root.nodelist.size

    template = Liquid::Template.parse("{{ -}} foo {{- }}")
    assert_equal 3, template.root.nodelist.size
  end

  def test_pre_trim
    template = Liquid::Template.parse("\n{%- raw %}{% endraw %}")
    assert_equal "", template.render

    template = Liquid::Template.parse("\n{%- if true %}{% endif %}")
    assert_equal "", template.render
  end
end
