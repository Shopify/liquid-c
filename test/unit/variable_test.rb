# encoding: utf-8
require 'test_helper'

class VariableTest < MiniTest::Unit::TestCase
  def test_variable_parse
    assert_equal ['hello', []], variable_parse(' hello ')
    assert_equal ['"what"', []], variable_parse(' "what" ')
    assert_equal ['hello["what"]', []], variable_parse(' hello["what"] ')
    assert_equal [nil, []], variable_parse('')
  end

  def test_strictness
    assert_raises Liquid::SyntaxError do
      variable_parse(' hello["what\']" ')
    end

    assert_raises Liquid::SyntaxError do
      variable_parse('-..')
    end
  end

  def test_variable_filter
    assert_equal ['name', [['filter', []]]], variable_parse(' name | filter ')
    assert_equal ['name', [['filter1', []], ['filter2', []]]], variable_parse(' name | filter1 | filter2 ')
  end

  def test_variable_filter_args
    assert_equal ['name', [['filter', ['aoeu']]]], variable_parse(' name | filter: aoeu ')
    assert_equal ['name', [['filter1', ['aoeu']], ['filter2', ['aoeu']]]], variable_parse(' name | filter1: aoeu | filter2: aoeu ')
    assert_equal ['name', [['filter', ['a', 'b: c', 'd: e']]]], variable_parse('name | filter : a , b : c , d : e')

    assert_raises Liquid::SyntaxError do
      assert_equal ['name', [['filter', ['a : b', 'c : d', 'e']]]], variable_parse('name | filter : a : b : c : d : e')
    end
  end

  def test_unicode
    assert_equal ['"å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ"', []], variable_parse('"å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ"')
  end

  private

  def variable_parse(markup)
    Liquid::Variable.c_strict_parse(markup)
  end
end
