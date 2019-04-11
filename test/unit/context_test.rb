require 'test_helper'
require 'bigdecimal'

class ContextTest < Minitest::Test
  def test_evaluate_works_with_normal_values
    context = Liquid::Context.new

    ["abc", 123, false, 1.21, BigDecimal(42)].each do |value|
      assert_equal value, context.evaluate(value)
    end

    assert_nil context.evaluate(nil)
  end

  def test_evaluate_works_with_classes_that_have_an_evaluate_method
    class_with_evaluate = Class.new do
      def evaluate(context)
        42
      end
    end

    assert_equal 42, Liquid::Context.new.evaluate(class_with_evaluate.new)
  end

  def test_evaluate_works_with_variable_lookup
    assert_equal 42, Liquid::Context.new({"var" => 42}).evaluate(Liquid::VariableLookup.new("var"))
  end

  def test_evaluating_a_variable_entirely_within_c
    context = Liquid::Context.new({"var" => 42})
    lookup = Liquid::VariableLookup.new("var")

    called_ruby_method_count = 0
    called_c_method_count = 0

    begin
      call_trace = TracePoint.trace(:call) do |t|
        called_ruby_method_count += 1
      end

      c_call_trace = TracePoint.trace(:c_call) do |t|
        called_c_method_count += 1 if t.method_id != :disable
      end

      context.evaluate(lookup)
    ensure
      call_trace.disable if call_trace
      c_call_trace.disable if c_call_trace
    end

    assert_equal 0, called_ruby_method_count
    assert_equal 1, called_c_method_count # context.evaluate call
  end
end
