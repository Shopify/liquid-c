# frozen_string_literal: true

require 'liquid'
require 'liquid/c'
require 'benchmark/ips'

liquid_lib_dir = $LOAD_PATH.detect { |p| File.exist?(File.join(p, 'liquid.rb')) }
require File.join(File.dirname(liquid_lib_dir), "performance/theme_runner.rb")

module SerializationThemeRunner
  def initialize
    super
    serialize_all_tests
  end

  def deserialize
    @serialized_tests.each do |test|
      Liquid::Template.load(test[:tmpl])
      Liquid::Template.load(test[:layout])
    end
  end

  def deserialize_and_render
    @serialized_tests.each do |test|
      tmpl    = test[:tmpl]
      assigns = test[:assigns]
      layout  = test[:layout]

      render_layout(Liquid::Template.load(tmpl), Liquid::Template.load(layout), assigns)
    end
  end

  private

  def serialize_all_tests
    @serialized_tests = []
    @compiled_tests.each do |test_hash|
      @serialized_tests <<
        if test_hash[:layout]
          { tmpl: test_hash[:tmpl].dump, assigns: test_hash[:assigns], layout: test_hash[:layout].dump }
        else
          { tmpl: test_hash[:tmpl].dump, assigns: test_hash[:assigns] }
        end
    end
    @serialized_tests
  end
end

ThemeRunner.prepend(SerializationThemeRunner)

Liquid::Template.error_mode = ARGV.first.to_sym
profiler = ThemeRunner.new

Benchmark.ips do |x|
  x.time   = 10
  x.warmup = 5

  puts
  puts "Running benchmark for #{x.time} seconds (with #{x.warmup} seconds warmup)."
  puts

  x.report("deserialize:") { profiler.deserialize }
  x.report("deserialize & render:") { profiler.deserialize_and_render }
end
