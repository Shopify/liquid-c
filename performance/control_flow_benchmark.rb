#!/usr/bin/env ruby
# frozen_string_literal: true

require "bundler/setup"
require "liquid/c"
require "benchmark/ips"

# Simple if/else
SIMPLE_IF = <<~LIQUID
{% if x == 1 %}one{% else %}other{% endif %}
LIQUID

# If/elsif/else chain
IF_ELSIF_CHAIN = <<~LIQUID
{% if x == 1 %}
  one
{% elsif x == 2 %}
  two
{% elsif x == 3 %}
  three
{% elsif x == 4 %}
  four
{% else %}
  other
{% endif %}
LIQUID

# Nested if statements
NESTED_IF = <<~LIQUID
{% if a %}
  {% if b %}
    {% if c %}
      deep
    {% else %}
      not c
    {% endif %}
  {% else %}
    not b
  {% endif %}
{% else %}
  not a
{% endif %}
LIQUID

# Simple case/when
SIMPLE_CASE = <<~LIQUID
{% case x %}
{% when 1 %}one
{% when 2 %}two
{% when 3 %}three
{% else %}other
{% endcase %}
LIQUID

# Case with many whens
CASE_MANY_WHENS = <<~LIQUID
{% case color %}
{% when "red" %}#FF0000
{% when "green" %}#00FF00
{% when "blue" %}#0000FF
{% when "yellow" %}#FFFF00
{% when "orange" %}#FFA500
{% when "purple" %}#800080
{% when "pink" %}#FFC0CB
{% when "black" %}#000000
{% when "white" %}#FFFFFF
{% else %}unknown
{% endcase %}
LIQUID

# Unless statement
UNLESS_TEMPLATE = <<~LIQUID
{% unless hidden %}
  visible content
{% endunless %}
{% unless disabled %}
  enabled content
{% else %}
  disabled content
{% endunless %}
LIQUID

# Mixed control flow
MIXED_CONTROL = <<~LIQUID
{% if show_header %}
  <header>{{ title }}</header>
{% endif %}
{% case status %}
{% when "active" %}
  {% if premium %}
    Premium Active
  {% else %}
    Standard Active
  {% endif %}
{% when "pending" %}
  Pending...
{% else %}
  Inactive
{% endcase %}
{% unless hidden %}
  Footer
{% endunless %}
LIQUID

puts "Testing CONTROL FLOW optimization (if/unless/case)"
puts "C VM enabled: #{ENV['LIQUID_C_DISABLE_VM'].nil?}"
puts "-" * 60

# Assigns for rendering
assigns_simple = { "x" => 2 }
assigns_nested = { "a" => true, "b" => true, "c" => false }
assigns_case = { "x" => 3, "color" => "blue" }
assigns_unless = { "hidden" => false, "disabled" => true }
assigns_mixed = { "show_header" => true, "title" => "Hello", "status" => "active", "premium" => true, "hidden" => false }

puts "\n=== PARSE-ONLY BENCHMARKS ==="
Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("parse: simple if") { Liquid::Template.parse(SIMPLE_IF) }
  x.report("parse: if/elsif chain") { Liquid::Template.parse(IF_ELSIF_CHAIN) }
  x.report("parse: nested if") { Liquid::Template.parse(NESTED_IF) }
  x.report("parse: simple case") { Liquid::Template.parse(SIMPLE_CASE) }
  x.report("parse: case many whens") { Liquid::Template.parse(CASE_MANY_WHENS) }
  x.report("parse: unless") { Liquid::Template.parse(UNLESS_TEMPLATE) }
  x.report("parse: mixed control") { Liquid::Template.parse(MIXED_CONTROL) }

  x.compare!
end

puts "\n=== PARSE+RENDER BENCHMARKS ==="
Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("parse+render: simple if") do
    Liquid::Template.parse(SIMPLE_IF).render(assigns_simple)
  end
  x.report("parse+render: if/elsif chain") do
    Liquid::Template.parse(IF_ELSIF_CHAIN).render(assigns_simple)
  end
  x.report("parse+render: nested if") do
    Liquid::Template.parse(NESTED_IF).render(assigns_nested)
  end
  x.report("parse+render: simple case") do
    Liquid::Template.parse(SIMPLE_CASE).render(assigns_case)
  end
  x.report("parse+render: case many whens") do
    Liquid::Template.parse(CASE_MANY_WHENS).render(assigns_case)
  end
  x.report("parse+render: unless") do
    Liquid::Template.parse(UNLESS_TEMPLATE).render(assigns_unless)
  end
  x.report("parse+render: mixed control") do
    Liquid::Template.parse(MIXED_CONTROL).render(assigns_mixed)
  end

  x.compare!
end

puts "\n=== RENDER-ONLY BENCHMARKS (pre-parsed) ==="
tpl_simple_if = Liquid::Template.parse(SIMPLE_IF)
tpl_elsif = Liquid::Template.parse(IF_ELSIF_CHAIN)
tpl_nested = Liquid::Template.parse(NESTED_IF)
tpl_simple_case = Liquid::Template.parse(SIMPLE_CASE)
tpl_case_many = Liquid::Template.parse(CASE_MANY_WHENS)
tpl_unless = Liquid::Template.parse(UNLESS_TEMPLATE)
tpl_mixed = Liquid::Template.parse(MIXED_CONTROL)

Benchmark.ips do |x|
  x.warmup = 2
  x.time = 5

  x.report("render: simple if") { tpl_simple_if.render(assigns_simple) }
  x.report("render: if/elsif chain") { tpl_elsif.render(assigns_simple) }
  x.report("render: nested if") { tpl_nested.render(assigns_nested) }
  x.report("render: simple case") { tpl_simple_case.render(assigns_case) }
  x.report("render: case many whens") { tpl_case_many.render(assigns_case) }
  x.report("render: unless") { tpl_unless.render(assigns_unless) }
  x.report("render: mixed control") { tpl_mixed.render(assigns_mixed) }

  x.compare!
end

puts "\nBenchmark complete!"
