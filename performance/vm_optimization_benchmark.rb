#!/usr/bin/env ruby
# frozen_string_literal: true

require "bundler/setup"
require "liquid/c"
require "benchmark/ips"

# Heavy increment template - 100 increments
INCREMENT_HEAVY = (["{% increment c %}"] * 100).join("\n")

# Heavy decrement template
DECREMENT_HEAVY = (["{% decrement d %}"] * 100).join("\n")

# Comment heavy template
COMMENT_HEAVY = (["{% comment %}some text{% endcomment %}"] * 100).join("\n")

# Mixed template with many tags
MIXED_HEAVY = (
  ["{% increment c %}"] * 30 +
  ["{% decrement d %}"] * 30 +
  ["{% comment %}text{% endcomment %}"] * 20 +
  ["{{ 'literal' }}"] * 20
).join("\n")

puts "VM Optimization Benchmark"
puts "C VM enabled: #{ENV['LIQUID_C_DISABLE_VM'].nil?}"
puts "-" * 50

# Pre-parse templates
inc_template = Liquid::Template.parse(INCREMENT_HEAVY)
dec_template = Liquid::Template.parse(DECREMENT_HEAVY)
comment_template = Liquid::Template.parse(COMMENT_HEAVY)
mixed_template = Liquid::Template.parse(MIXED_HEAVY)

Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("100 increments") do
    inc_template.render
  end

  x.report("100 decrements") do
    dec_template.render
  end

  x.report("100 comments") do
    comment_template.render
  end

  x.report("100 mixed tags") do
    mixed_template.render
  end

  x.compare!
end
