# encoding: utf-8
require 'test_helper'

class TokenizerTest < MiniTest::Unit::TestCase
  def test_tokenize_strings
    assert_equal [' '], tokenize(' ')
    assert_equal ['hello world'], tokenize('hello world')
  end

  def test_tokenize_variables
    assert_equal ['{{funk}}'], tokenize('{{funk}}')
    assert_equal [' ', '{{funk}}', ' '], tokenize(' {{funk}} ')
    assert_equal [' ', '{{funk}}', ' ', '{{so}}', ' ', '{{brother}}', ' '], tokenize(' {{funk}} {{so}} {{brother}} ')
    assert_equal [' ', '{{  funk  }}', ' '], tokenize(' {{  funk  }} ')
  end

  def test_tokenize_blocks
    assert_equal ['{%comment%}'], tokenize('{%comment%}')
    assert_equal [' ', '{%comment%}', ' '], tokenize(' {%comment%} ')

    assert_equal [' ', '{%comment%}', ' ', '{%endcomment%}', ' '], tokenize(' {%comment%} {%endcomment%} ')
    assert_equal ['  ', '{% comment %}', ' ', '{% endcomment %}', ' '], tokenize("  {% comment %} {% endcomment %} ")
  end

  def test_utf8_encoded_template
    source = 'auswählen'
    assert_equal Encoding::UTF_8, source.encoding
    output = tokenize(source)
    assert_equal [Encoding::UTF_8], output.map(&:encoding)
    assert_equal [source], output
  end

  private

  def tokenize(source)
    tokenizer = Liquid::C::Tokenizer.new(source, false)
    tokens = []
    while t = tokenizer.shift
      tokens << t
    end
    tokens
  end
end
