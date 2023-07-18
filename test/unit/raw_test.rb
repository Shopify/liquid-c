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
    output = Liquid::Template.parse("{% raw_wrapper %}body{% endraw_wrapper %}").render!
    assert_equal("<body>", output)
  end

  def test_does_not_allow_nbsp_in_tag_delimiter
    # these are valid
    Liquid::Template.parse("{% raw %}body{% endraw\u00A0%}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0%}")
    Liquid::Template.parse("{% raw %}body{% endraw\u00A0 %}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0 %}")
    Liquid::Template.parse("{% raw %}body{% endraw \u00A0 endraw %}")

    [
      "{%\u00A0endraw%}",
      "{%\u00A0 endraw%}",
      "{% \u00A0endraw%}",
      "{% \u00A0 endraw%}",
      "{%\u00A0endraw\u00A0%}",
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
