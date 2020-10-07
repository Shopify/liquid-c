# encoding: utf-8
require 'test_helper'

class VariableTest < Minitest::Test
  def test_variable_parse
    assert_equal [lookup('hello'), []], variable_parse('hello')
    assert_equal ['world', []], variable_parse(' "world" ')
    assert_equal [lookup('hello["world"]'), []], variable_parse(' hello["world"] ')
    assert_equal [nil, []], variable_parse('')
    assert_equal [lookup('question?'), []], variable_parse('question?')
    assert_equal [lookup('[meta]'), []], variable_parse('[meta]')
    assert_equal [lookup('a-b'), []], variable_parse('a-b')
    assert_equal [lookup('a-2'), []], variable_parse('a-2')
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
    assert_equal [123.4, []], variable_parse('123.4')

    assert_equal [lookup('[blank]'), []], variable_parse('[blank]')
    assert_equal [lookup(false, true, [Liquid::Expression::LITERALS['blank']], 0), []], variable_parse('[true][blank]')
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

  def test_broken_unicode_errors
    err = assert_raises(Liquid::SyntaxError) do
      Liquid::Template.parse("test {{ \xC2\xA0 test }}", error_mode: :strict)
    end
    assert err.message
  end

  def test_callbacks
    variable_parses = 0
    variable_fallbacks = 0

    callbacks = {
      variable_fallback: lambda { variable_fallbacks += 1 }
    }

    create_variable('abc', error_mode: :lax, stats_callbacks: callbacks)
    assert_equal 0, variable_fallbacks

    create_variable('@!#', error_mode: :lax, stats_callbacks: callbacks)
    assert_equal 1, variable_fallbacks
  end

  def test_write_string
    output = Liquid::Template.parse("{{ str }}").render({ 'str' => 'foo' })
    assert_equal "foo", output
  end

  def test_write_fixnum
    output = Liquid::Template.parse("{{ num }}").render({ 'num' => 123456 })
    assert_equal "123456", output
  end

  def test_write_array
    output = Liquid::Template.parse("{{ ary }}").render({ 'ary' => ['foo', 123, ['nested', 'ary'], nil, 0.5] })
    assert_equal "foo123nestedary0.5", output
  end

  def test_write_nil
    output = Liquid::Template.parse("{{ obj }}").render({ 'obj' => nil })
    assert_equal "", output
  end

  class StringifiableObject
    def initialize(as_string)
      @as_string = as_string
    end

    def to_s
      @as_string
    end

    def to_liquid
      self
    end
  end

  def test_write_unknown_object
    output = Liquid::Template.parse("{{ obj }}").render({ 'obj' => StringifiableObject.new('foo') })
    assert_equal "foo", output
  end

  def test_filter_without_args
    output = Liquid::Template.parse("{{ var | upcase }}").render({ 'var' => 'Hello' })
    assert_equal "HELLO", output
  end

  def test_filter_with_const_arg
    output = Liquid::Template.parse("{{ x | plus: 2 }}").render({ 'x' => 3 })
    assert_equal "5", output
  end

  def test_filter_with_variable_arg
    output = Liquid::Template.parse("{{ x | plus: y }}").render({ 'x' => 10, 'y' => 123 })
    assert_equal "133", output
  end

  def test_filter_with_variable_arg_after_const_arg
    output = Liquid::Template.parse("{{ ary | slice: 1, 2 }}").render({ 'ary' => [1, 2, 3, 4] })
    assert_equal "23", output
  end

  def test_filter_with_const_keyword_arg
    output = Liquid::Template.parse("{{ value | default: 'None' }}").render({ 'value' => false })
    assert_equal 'None', output

    output = Liquid::Template.parse("{{ value | default: 'None', allow_false: true }}").render({ 'value' => false })
    assert_equal 'false', output
  end

  def test_filter_with_variable_keyword_arg
    template = Liquid::Template.parse("{{ value | default: 'None', allow_false: false_allowed }}")

    assert_equal 'None', template.render({ 'value' => false, 'false_allowed' => false })
    assert_equal 'false', template.render({ 'value' => false, 'false_allowed' => true })
  end

  def test_filter_error
    output = Liquid::Template.parse("before ({{ ary | concat: 2 }}) after").render({ 'ary' => [1] })
    assert_equal 'before (Liquid error: concat filter requires an array argument) after', output
  end

  private

  def create_variable(markup, options={})
    Liquid::Variable.new(markup, Liquid::ParseContext.new(options))
  end

  def variable_parse(markup)
    name = Liquid::Variable.c_strict_parse(markup, filters = [])
    [name, filters]
  end

  def lookup(*args)
    Liquid::VariableLookup.new(*args)
  end
end
