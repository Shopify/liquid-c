# encoding: utf-8
# frozen_string_literal: true
require 'test_helper'

class TokenizerTest < Minitest::Test
  def test_tokenize_strings
    assert_equal([' '], tokenize(' '))
    assert_equal(['hello world'], tokenize('hello world'))
  end

  def test_tokenize_variables
    assert_equal(['{{funk}}'], tokenize('{{funk}}'))
    assert_equal([' ', '{{funk}}', ' '], tokenize(' {{funk}} '))
    assert_equal([' ', '{{funk}}', ' ', '{{so}}', ' ', '{{brother}}', ' '], tokenize(' {{funk}} {{so}} {{brother}} '))
    assert_equal([' ', '{{  funk  }}', ' '], tokenize(' {{  funk  }} '))

    # Doesn't strip whitespace
    assert_equal([' ', '  funk  ', ' '], tokenize(' {{  funk  }} ', trimmed: true))
  end

  def test_tokenize_blocks
    assert_equal(['{%comment%}'], tokenize('{%comment%}'))
    assert_equal([' ', '{%comment%}', ' '], tokenize(' {%comment%} '))

    assert_equal([' ', '{%comment%}', ' ', '{%endcomment%}', ' '], tokenize(' {%comment%} {%endcomment%} '))
    assert_equal(['  ', '{% comment %}', ' ', '{% endcomment %}', ' '], tokenize("  {% comment %} {% endcomment %} "))

    # Doesn't strip whitespace
    assert_equal([' ', '  comment  ', ' '], tokenize(' {%  comment  %} ', trimmed: true))
  end

  def test_tokenize_for_liquid_tag
    source = "\nfunk\n\n  so | brother   \n"

    assert_equal(["\nfunk\n\n  ", "so | brother   \n"], tokenize(source, for_liquid_tag: true))

    # Strips whitespace
    assert_equal(["funk", "so | brother"], tokenize(source, for_liquid_tag: true, trimmed: true))
  end

  def test_utf8_encoded_source
    source = 'auswählen'
    assert_equal(Encoding::UTF_8, source.encoding)
    output = tokenize(source)
    assert_equal([Encoding::UTF_8], output.map(&:encoding))
    assert_equal([source], output)
  end

  def test_utf8_compatible_source
    source = String.new('ascii', encoding: Encoding::ASCII)
    tokenizer = Liquid::Tokenizer.new(source)
    output = tokenizer.shift
    assert_equal(Encoding::UTF_8, output.encoding)
    assert_equal(source, output)
    assert_nil(tokenizer.shift)
  end

  def test_non_utf8_compatible_source
    source = 'üñicode'.dup.force_encoding(Encoding::BINARY)
    exc = assert_raises(Encoding::CompatibilityError) do
      Liquid::C::Tokenizer.new(source, 1, false)
    end
    assert_equal("non-UTF8 encoded source (ASCII-8BIT) not supported", exc.message)
  end

  def test_source_too_large
    too_large_source = "a" * 2**24
    max_length_source = too_large_source.chop

    # C safety check
    err = assert_raises(ArgumentError) do
      Liquid::C::Tokenizer.new(too_large_source, 1, false)
    end
    assert_match(/Source too large, max \d+ bytes/, err.message)

    # ruby patch fallback
    liquid_c_tokenizer = Liquid::Tokenizer.new(max_length_source)
    assert_instance_of(Liquid::C::Tokenizer, liquid_c_tokenizer)
    fallback_tokenizer = Liquid::Tokenizer.new(too_large_source)
    assert_instance_of(Liquid::Tokenizer, fallback_tokenizer)

    # Document.parse patch parse context update
    parse_context = Liquid::ParseContext.new
    refute(parse_context.send(:disable_liquid_c_nodes))
    Liquid::Document.parse(liquid_c_tokenizer, parse_context)
    refute(parse_context.send(:disable_liquid_c_nodes))
    Liquid::Document.parse(fallback_tokenizer, parse_context)
    assert_equal(true, parse_context.send(:disable_liquid_c_nodes))
  end

  private

  def tokenize(source, for_liquid_tag: false, trimmed: false)
    tokenizer = Liquid::C::Tokenizer.new(source, 1, for_liquid_tag)
    tokens = []
    while (t = trimmed ? tokenizer.send(:shift_trimmed) : tokenizer.shift)
      tokens << t
    end
    tokens
  end
end
