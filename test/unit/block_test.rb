require 'test_helper'

class BlockTest < MiniTest::Test
  def test_no_allocation_of_trimmed_strings
    template = Liquid::Template.parse("{{ -}}     {{- }}")
    assert_equal 2, template.root.nodelist.size

    template = Liquid::Template.parse("{{ -}} foo {{- }}")
    assert_equal 3, template.root.nodelist.size
  end

  def test_raise_on_output_with_non_utf8_encoding
    output = String.new(encoding: Encoding::BINARY)
    template = Liquid::Template.parse("ascii text")
    exc = assert_raises(Encoding::CompatibilityError) do
      template.render!({}, output: output)
    end
    assert_equal("non-UTF8 encoded output (ASCII-8BIT) not supported", exc.message)
  end

  def test_write_unicode_characters
    output = String.new(encoding: Encoding::UTF_8)
    template = Liquid::Template.parse("ü{{ unicode_char }}")
    assert_equal("üñ", template.render!({ 'unicode_char' => 'ñ' }, output: output))
  end
end
