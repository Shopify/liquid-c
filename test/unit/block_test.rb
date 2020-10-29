# frozen_string_literal: true
require 'test_helper'

class BlockTest < MiniTest::Test
  def test_no_allocation_of_trimmed_strings
    template = Liquid::Template.parse("{{ a -}}     {{- b }}")
    assert_equal(2, template.root.nodelist.size)

    template = Liquid::Template.parse("{{ a -}} foo {{- b }}")
    assert_equal(3, template.root.nodelist.size)
  end

  def test_raise_on_output_with_non_utf8_encoding
    output = String.new(encoding: Encoding::ASCII)
    template = Liquid::Template.parse("ascii text")
    exc = assert_raises(Encoding::CompatibilityError) do
      template.render!({}, output: output)
    end
    assert_equal("non-UTF8 encoded output (US-ASCII) not supported", exc.message)
  end

  def test_write_unicode_characters
    output = String.new(encoding: Encoding::UTF_8)
    template = Liquid::Template.parse("ü{{ unicode_char }}")
    assert_equal("üñ", template.render!({ 'unicode_char' => 'ñ' }, output: output))
  end

  def test_op_write_raw_w
    output = "a" * 2**8
    template = Liquid::Template.parse(output)
    assert_equal(output, template.render!)
  end

  def test_disassemble
    source = <<~LIQUID
      raw
      {{- var | default: "none", allow_false: true -}}
      {%- increment counter -%}
    LIQUID
    template = Liquid::Template.parse(source, line_numbers: true)
    block_body = template.root.body
    increment_node = block_body.nodelist[2]
    assert_instance_of(Liquid::Increment, increment_node)
    assert_equal(<<~ASM, block_body.disassemble)
      0x0000: write_raw("raw")
      0x0001: render_variable_rescue(line_number: 2)
      0x0005: find_static_var("var")
      0x0006: push_const("none")
      0x0007: push_const("allow_false")
      0x0008: push_true
      0x0009: hash_new(1)
      0x000b: filter(name: :default, num_args: 3)
      0x000d: pop_write
      0x000e: write_node(#{increment_node.inspect})
      0x000f: leave
    ASM
  end
end
