#!/usr/bin/env ruby
# frozen_string_literal: true

require "bundler/setup"
require "liquid/c"
require "benchmark/ips"

# Template with many increment/decrement operations
INCREMENT_TEMPLATE = <<~LIQUID
{% increment counter %}
{% increment counter %}
{% increment counter %}
{% decrement other %}
{% decrement other %}
{% increment counter %}
{% decrement other %}
{% increment counter %}
{% increment counter %}
{% decrement other %}
LIQUID

# Template mixing increment with other tags
MIXED_TEMPLATE = <<~LIQUID
{% assign x = 1 %}
{% increment counter %}
{% if x == 1 %}yes{% endif %}
{% increment counter %}
{% for i in (1..3) %}{{ i }}{% endfor %}
{% decrement other %}
{% increment counter %}
LIQUID

puts "Testing INCREMENT/DECREMENT optimization"
puts "C VM enabled: #{ENV['LIQUID_C_DISABLE_VM'].nil?}"
puts "-" * 50

Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("parse+render: increment heavy") do
    template = Liquid::Template.parse(INCREMENT_TEMPLATE)
    template.render
  end

  x.report("parse+render: mixed with increment") do
    template = Liquid::Template.parse(MIXED_TEMPLATE)
    template.render
  end

  x.report("render only: increment heavy") do
    @inc_tpl ||= Liquid::Template.parse(INCREMENT_TEMPLATE)
    @inc_tpl.render
  end

  x.compare!
end
