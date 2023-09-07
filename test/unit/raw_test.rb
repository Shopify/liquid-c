# frozen_string_literal: true

require "test_helper"

class RawTest < Minitest::Test
  class RawWrapper < Liquid::Raw
    def render_to_output_buffer(_context, output)
      output << "<"
      super
      output << ">"
    end
  end
  Liquid::Template.register_tag("raw_wrapper", RawWrapper)

  def test_derived_class
    [
      "{% raw_wrapper %}body{% endraw_wrapper %}",
      "{% raw_wrapper %}body{%endraw_wrapper%}",
      "{% raw_wrapper %}body{%- endraw_wrapper -%}",
      "{% raw_wrapper %}body{%- endraw_wrapper %}",
      "{% raw_wrapper %}body{% endraw_wrapper -%}",
    ].each do |template|
      output = Liquid::Template.parse(template).render!

      assert_equal(
        "<body>",
        output,
        "Template: #{template}"
      )
    end
  end

  def test_allows_extra_string_after_tag_delimiter
    output = Liquid::Template.parse("{% raw %}message{% endraw this_is_allowed %}").render
    assert_equal("message", output)

    output = Liquid::Template.parse("{% raw %}message{%   endraw r%}").render
    assert_equal("message", output)
  end

  def test_ignores_incomplete_tag_delimter
    output = Liquid::Template.parse("{% raw %}{% endraw {% endraw %}").render
    assert_equal("{% endraw ", output)

    output = Liquid::Template.parse("{% raw %}{%endraw{% endraw %}").render
    assert_equal("{%endraw", output)

    output = Liquid::Template.parse("{% raw %}{%- endraw {% endraw %}").render
    assert_equal("{%- endraw ", output)
  end

  def test_does_not_allow_nbsp_in_tag_delimiter
    # these are valid
    Liquid::Template.parse("{% raw %}body{%endraw%}")
    Liquid::Template.parse("{% raw %}body{% endraw-%}")
    Liquid::Template.parse("{% raw %}body{% endraw -%}")
    Liquid::Template.parse("{% raw %}body{%-endraw %}")
    Liquid::Template.parse("{% raw %}body{%- endraw %}")
    Liquid::Template.parse("{% raw %}body{%-endraw-%}")
    Liquid::Template.parse("{% raw %}body{%- endraw -%}")
    Liquid::Template.parse("{% raw %}body{% endraw\u00A0%}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0%}")
    Liquid::Template.parse("{% raw %}body{% endraw\u00A0 %}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0 %}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0 endraw %}")
    Liquid::Template.parse("{% raw %}body{% endraw\u00A0endraw %}")

    [
      "{%\u00A0endraw%}",
      "{%\u00A0 endraw%}",
      "{% \u00A0endraw%}",
      "{% \u00A0 endraw%}",
      "{%\u00A0endraw\u00A0%}",
      "{% - endraw %}",
      "{% endnot endraw %}",
    ].each do |bad_delimiter|
      exception = assert_raises(
        Liquid::SyntaxError,
        "#{bad_delimiter.inspect} did not raise Liquid::SyntaxError"
      ) do
        Liquid::Template.parse(
          "{% raw %}body#{bad_delimiter}"
        )
      end

      assert_equal(
        exception.message,
        "Liquid syntax error: 'raw' tag was never closed",
        "#{bad_delimiter.inspect} raised the wrong exception message",
      )
    end
  end
end
