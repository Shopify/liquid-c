#!/usr/bin/env ruby
# frozen_string_literal: true

require "bundler/setup"
require "liquid/c"
require "benchmark/ips"

# Heavy increment template - 100 increments
INCREMENT_HEAVY = (["{% increment c %}"] * 100).join("\n")

# Heavy decrement template
DECREMENT_HEAVY = (["{% decrement d %}"] * 100).join("\n")

# Mixed template
MIXED_HEAVY = (
  ["{% increment c %}"] * 50 +
  ["{% decrement d %}"] * 50
).join("\n")

puts "Parse+Render Optimization Benchmark"
puts "C VM enabled: #{ENV['LIQUID_C_DISABLE_VM'].nil?}"
puts "-" * 50

Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("parse+render: 100 increments") do
    template = Liquid::Template.parse(INCREMENT_HEAVY)
    template.render
  end

  x.report("parse+render: 100 decrements") do
    template = Liquid::Template.parse(DECREMENT_HEAVY)
    template.render
  end

  x.report("parse+render: 100 mixed") do
    template = Liquid::Template.parse(MIXED_HEAVY)
    template.render
  end

  x.compare!
end
