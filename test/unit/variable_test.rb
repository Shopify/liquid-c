# encoding: utf-8
require 'test_helper'

class VariableTest < MiniTest::Unit::TestCase
  def test_variable_parse
    assert_equal [lookup('hello'), []], variable_parse('hello')
    assert_equal ['world', []], variable_parse(' "world" ')
    assert_equal [lookup('hello["world"]'), []], variable_parse(' hello["world"] ')
    assert_equal [nil, []], variable_parse('')
    assert_equal [lookup('question?'), []], variable_parse('question?')
    assert_equal [lookup('[meta]'), []], variable_parse('[meta]')
    assert_equal [lookup('a-b'), []], variable_parse('a-b')
  end

  def test_strictness
    assert_raises(Liquid::SyntaxError) { variable_parse(' hello["world\']" ') }
    assert_raises(Liquid::SyntaxError) { variable_parse('-..') }
    assert_raises(Liquid::SyntaxError) { variable_parse('question?mark') }
    assert_raises(Liquid::SyntaxError) { variable_parse('123.foo') }
    assert_raises(Liquid::SyntaxError) { variable_parse(' | nothing') }

    ['a .b', 'a. b', 'a . b'].each do |var|
      assert_raises(Liquid::SyntaxError) { variable_parse(var) }
    end

    ['a -b', 'a- b', 'a - b'].each do |var|
      assert_raises(Liquid::SyntaxError) { variable_parse(var) }
    end
  end

  def test_literals
    assert_equal [true, []], variable_parse('true')
    assert_equal [nil, []], variable_parse('nil')

    assert_equal [lookup('[blank]'), []], variable_parse('[blank]')
    assert_equal [lookup(false, true, [:blank?], 0), []], variable_parse('[true][blank]')
    assert_equal [lookup('[true][blank]'), []], variable_parse('[true][blank]')
    assert_equal [lookup('x["size"]'), []], variable_parse('x["size"]')
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

    assert_equal [name, [['filter1', [abc]], ['filter2', [abc]]]],
      variable_parse(' name | filter1: abc | filter2: abc ')

    assert_equal [name, [['filter', [lookup('a')], {'b' => lookup('c'), 'd' => lookup('e')}]]],
      variable_parse('name | filter : a , b : c , d : e')

    assert_raises Liquid::SyntaxError do
      variable_parse('name | filter : a : b : c : d : e')
    end
  end

  def test_unicode_strings
    assert_equal ['å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒ', []],
      variable_parse('"å߀êùｉｄｈｔлｓԁѵ߀ｒáƙìｓｔɦｅƅêｓｔｐｃｍáѕｔｅｒｒãｃêｃհèｒｒ"')
  end

  def test_callbacks
    variable_parses = 0
    variable_fallbacks = 0

    callbacks = {
      variable_parse: lambda { variable_parses += 1 },
      variable_fallback: lambda { variable_fallbacks += 1 }
    }

    Liquid::Variable.new('abc', error_mode: :lax, stats_callbacks: callbacks)
    assert_equal 1, variable_parses
    assert_equal 0, variable_fallbacks

    Liquid::Variable.new('@!#', error_mode: :lax, stats_callbacks: callbacks)
    assert_equal 2, variable_parses
    assert_equal 1, variable_fallbacks
  end

  private

  def variable_parse(markup)
    Liquid::Variable.c_strict_parse(markup)
  end

  def lookup(*args)
    Liquid::VariableLookup.new(*args)
  end
end
