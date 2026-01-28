# encoding: utf-8
# frozen_string_literal: true

require "test_helper"

# Help catch bugs from objects not being marked at all
# GC opportunities.
class GCStressTest < Minitest::Test
  def setup
    skip "GC stress tests disabled; set LIQUID_C_GC_STRESS=1 to enable" unless ENV["LIQUID_C_GC_STRESS"] == "1"
  end

  def test_compile_and_render
    source = "{% assign x = 1 %}{% if x -%} x: {{ x | plus: 2 }}{% endif %}"
    result = gc_stress do
      Liquid::Template.parse(source).render!
    end
    assert_equal("x: 3", result)
  end

  private

  def gc_stress
    old_value = GC.stress
    GC.stress = true
    begin
      yield
    ensure
      GC.stress = old_value
    end
  end
end
