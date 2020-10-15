require 'test_helper'

class ExpressionTest < MiniTest::Test
  def test_literals
    assert_equal true, Liquid::C::Expression.strict_parse('true')
    assert_equal false, Liquid::C::Expression.strict_parse('false')
    assert_equal nil, Liquid::C::Expression.strict_parse('nil')
    assert_equal nil, Liquid::C::Expression.strict_parse('null')

    empty = Liquid::C::Expression.strict_parse('empty')
    assert_equal '', empty
    assert_same empty, Liquid::C::Expression.strict_parse('blank')
  end

  def test_byte_int
    assert_equal 127, Liquid::C::Expression.strict_parse('127')
    assert_equal -128, Liquid::C::Expression.strict_parse('-128')
  end

  def test_short_int
    assert_equal 128, Liquid::C::Expression.strict_parse('128')
    assert_equal -129, Liquid::C::Expression.strict_parse('-129')
    assert_equal 32767, Liquid::C::Expression.strict_parse('32767')
    assert_equal -32768, Liquid::C::Expression.strict_parse('-32768')
  end

  def test_float
    assert_equal 123.4, Liquid::C::Expression.strict_parse('123.4')
  end

  def test_string
    assert_equal "hello", Liquid::C::Expression.strict_parse('"hello"')
    assert_equal "world", Liquid::C::Expression.strict_parse("'world'")
  end

  def test_find_static_variable
    context = Liquid::Context.new({"x" => 123})
    expr = Liquid::C::Expression.strict_parse('x')

    assert_instance_of(Liquid::C::Expression, expr)
    assert_equal 123, context.evaluate(expr)
  end

  def test_find_dynamic_variable
    context = Liquid::Context.new({"x" => "y", "y" => 42})
    expr = Liquid::C::Expression.strict_parse('[x]')
    assert_equal 42, context.evaluate(expr)
  end

  def test_find_missing_variable
    context = Liquid::Context.new({})
    expr = Liquid::C::Expression.strict_parse('missing')

    assert_nil context.evaluate(expr)

    context.strict_variables = true

    assert_raises(Liquid::UndefinedVariable) do
      context.evaluate(expr)
    end
  end

  def test_lookup_const_key
    context = Liquid::Context.new({"obj" => { "prop" => "some value" }})

    expr = Liquid::C::Expression.strict_parse('obj.prop')
    assert_equal 'some value', context.evaluate(expr)

    expr = Liquid::C::Expression.strict_parse('obj["prop"]')
    assert_equal 'some value', context.evaluate(expr)
  end

  def test_lookup_variable_key
    context = Liquid::Context.new({"field_name" => "prop", "obj" => { "prop" => "another value" }})
    expr = Liquid::C::Expression.strict_parse('obj[field_name]')
    assert_equal 'another value', context.evaluate(expr)
  end

  def test_lookup_command
    context = Liquid::Context.new({"ary" => ['a', 'b', 'c']})
    assert_equal 3, context.evaluate(Liquid::C::Expression.strict_parse('ary.size'))
    assert_equal 'a', context.evaluate(Liquid::C::Expression.strict_parse('ary.first'))
    assert_equal 'c', context.evaluate(Liquid::C::Expression.strict_parse('ary.last'))
  end

  def test_lookup_missing_key
    context = Liquid::Context.new({ 'obj' => {} })
    expr = Liquid::C::Expression.strict_parse('obj.missing')

    assert_nil context.evaluate(expr)

    context.strict_variables = true

    assert_raises(Liquid::UndefinedVariable) do
      context.evaluate(expr)
    end
  end

  def test_const_range
    assert_equal (1..2), Liquid::C::Expression.strict_parse('(1..2)')
  end

  def test_dynamic_range
    context = Liquid::Context.new({"var" => 42})
    expr = Liquid::C::Expression.strict_parse('(1..var)')
    assert_instance_of(Liquid::C::Expression, expr)
    assert_equal (1..42), context.evaluate(expr)
  end
end
