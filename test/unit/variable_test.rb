# encoding: utf-8
require 'test_helper'

class VariableTest < MiniTest::Unit::TestCase
  def test_variable_parse
    assert_equal ['hello', []], variable_parse(' hello ')
    assert_equal ['"what"', []], variable_parse(' "what" ')
    assert_equal ['hello["what"]', []], variable_parse(' hello["what"] ')
    assert_equal [nil, []], variable_parse('')
    assert_equal ['hello["what\']"', []], variable_parse(' hello["what\']" ')
  end

  def test_variable_filter
    assert_equal ['name', [['filter', []]]], variable_parse(' name | filter ')
    assert_equal ['name', [['filter1', []], ['filter2', []]]], variable_parse(' name | filter1 | filter2 ')
  end

  def test_variable_filter_args
    assert_equal ['name', [['filter', ['aoeu']]]], variable_parse(' name | filter: aoeu ')
    assert_equal ['name', [['filter1', ['aoeu']], ['filter2', ['aoeu']]]], variable_parse(' name | filter1: aoeu | filter2: aoeu ')
    assert_equal ['name', [['filter', ['a : b', 'c : d', 'e']]]], variable_parse('name | filter : a : b : c : d : e')
    assert_equal ['name', [['filter', ['a', 'b : c', 'd : e']]]], variable_parse('name | filter : a , b : c , d : e')
  end

  def test_unicode
    assert_equal ['å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ', []], variable_parse('å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ')
  end

  # contrived examples to test edge cases
  def test_parsing_quirks
    # pipe and commas before the filter name are ignored, as well as unterminated quote characters
    assert_equal ['name', []], variable_parse(%{ |'",name })

    # ignore anything between the filter name and the filter seperator
    assert_equal ['name', [['filter', []]]], variable_parse(%{ name ignored | filter })

    # ignore filters without word characters
    assert_equal ['name', [['filter', []]]], variable_parse(%{ name | "" | '' | , | || ? | filter })

    # filter name is the first consecutive word characters
    filters = [
      ['quote', []],
      ['question', []],
      ['arg', ['arg']]
    ]
    assert_equal ['name', filters], variable_parse(%{ name | 'quote' | question? | ???: arg })

    # ignore characters between the filter argument and the filter seperator
    assert_equal ['name', [['filter', ['arg1', 'arg2']]]], variable_parse(%{ name | filter: arg1 ignored, arg2 })

    # unquoted strings may be used as filter seperators
    filters = [
      ['filter1', []],
      ['filter2', []],
      ['filter3', []],
    ]
    assert_equal ['name', filters], variable_parse(%{name | filter1 " filter2 ' filter3})
  end

  private

  def variable_parse(markup)
    result = Liquid::Variable.new(markup)
    [ result.name, result.filters ]
  end
end
