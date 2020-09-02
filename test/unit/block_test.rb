require 'test_helper'

class BlockTest < MiniTest::Test
  def test_no_instruction_for_trimmed_strings
    context = Liquid::Context.new({ 'x' => '' })
    template = Liquid::Template.parse("{{ x -}}     {{- x }}")
    template.render(context)
    # render_score reflects the number of liquid equivalent nodes are rendered
    # which shouldn't show a node being rendered for the trimmed string
    assert_equal 2, context.resource_limits.render_score

    context.resource_limits.reset
    template = Liquid::Template.parse("{{ x -}} foo {{- x }}")
    template.render(context)
    assert_equal 3, context.resource_limits.render_score
  end

  def test_pre_trim
    template = Liquid::Template.parse("\n{%- raw %}{% endraw %}")
    assert_equal "", template.render

    template = Liquid::Template.parse("\n{%- if true %}{% endif %}")
    assert_equal "", template.render
  end

  # Temporary to test rollout of the fix for this bug
  def test_bug_compatible_pre_trim
    template = Liquid::Template.parse("\n {%- raw %}{% endraw %}", bug_compatible_whitespace_trimming: true)
    assert_equal "\n", template.render

    template = Liquid::Template.parse("\n {%- if true %}{% endif %}", bug_compatible_whitespace_trimming: true)
    assert_equal "\n", template.render

    template = Liquid::Template.parse("{{ 'B' }} \n{%- if true %}C{% endif %}", bug_compatible_whitespace_trimming: true)
    assert_equal "B C", template.render

    template = Liquid::Template.parse("B\n {%- raw %}{% endraw %}", bug_compatible_whitespace_trimming: true)
    assert_equal "B", template.render

    template = Liquid::Template.parse("B\n {%- if true %}{% endif %}", bug_compatible_whitespace_trimming: true)
    assert_equal "B", template.render
  end
end
