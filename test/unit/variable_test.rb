# encoding: utf-8
require 'test_helper'

class VariableTest < MiniTest::Unit::TestCase
  def test_variable_parse
    assert_equal [lookup('hello'), []], variable_parse('hello')
    assert_equal ['world', []], variable_parse(' "world" ')
    assert_equal [lookup('hello["world"]'), []], variable_parse(' hello["world"] ')
    assert_equal [nil, []], variable_parse('')
    assert_equal [lookup('question?'), []], variable_parse('question?')
  end

  def test_strictness
    assert_raises Liquid::SyntaxError do
      variable_parse(' hello["world\']" ')
    end

    assert_raises Liquid::SyntaxError do
      variable_parse('-..')
    end

    assert_raises Liquid::SyntaxError do
      variable_parse('question?mark')
    end
  end

  def test_literals
    assert_equal [true, []], variable_parse('true')
    assert_equal [nil, [['filter', []]]], variable_parse(' | filter')
  end

  def test_variable_filter
    name = lookup('name')
    assert_equal [name, [['filter', []]]], variable_parse(' name | filter ')
    assert_equal [name, [['filter1', []], ['filter2', []]]], variable_parse(' name | filter1 | filter2 ')
  end

  def test_variable_filter_args
    name = lookup('name')
    abc = lookup('abc')

    assert_equal [name, [['filter', [abc]]]], variable_parse(' name | filter: abc ')
    assert_equal [name, [['filter1', [abc]], ['filter2', [abc]]]], variable_parse(' name | filter1: abc | filter2: abc ')
    assert_equal [name, [[
      'filter',
      [lookup('a')],
      {'b' => lookup('c'), 'd' => lookup('e')}
    ]]], variable_parse('name | filter : a , b : c , d : e')

    assert_raises Liquid::SyntaxError do
      variable_parse('name | filter : a : b : c : d : e')
    end
  end

  def test_unicode_strings
    out = variable_parse('"å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ"')
    assert_equal ['å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒϒｍХƃｒｏɯлｔɦëｑüｉｃｋƅｒòｗԉｆòｘյｕｍρѕ߀ѵëｒｔɦëｌâｚϒｄ߀ɢ', []], out
  end

  private

  def variable_parse(markup)
    Liquid::Variable.c_strict_parse(markup)
  end

  def lookup(name)
    Liquid::VariableLookup.new(name)
  end
end
