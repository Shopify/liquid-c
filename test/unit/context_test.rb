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
end
