# frozen_string_literal: true

# Performance benchmarks comparing C parser to Ruby parser
#
# Run with:
#   bundle exec ruby performance/parser_benchmark.rb
#
# To compare with Ruby-only parsing:
#   LIQUID_C_DISABLE_VM=1 bundle exec ruby performance/parser_benchmark.rb

require "bundler/setup"
require "liquid"
require "liquid/c"
require "benchmark/ips"

# Check if C parsing is disabled
c_disabled = ENV["LIQUID_C_DISABLE_VM"] == "1"
if c_disabled
  puts "Running with Liquid-C VM DISABLED (Ruby parsing)"
  Liquid::ParseContext.liquid_c_nodes_disabled = true
else
  puts "Running with Liquid-C VM ENABLED (C parsing)"
end

puts "-" * 60

#-------------------------------------------------------------------------------
# Benchmark Templates
#-------------------------------------------------------------------------------

SIMPLE_IF = "{% if condition %}yes{% else %}no{% endif %}"

NESTED_IF = <<~LIQUID
  {% if a %}
    {% if b %}
      {% if c %}
        {% if d %}
          deep
        {% endif %}
      {% endif %}
    {% endif %}
  {% endif %}
LIQUID

SIMPLE_FOR = "{% for i in (1..10) %}{{ i }}{% endfor %}"

NESTED_FOR = <<~LIQUID
  {% for i in (1..5) %}
    {% for j in (1..5) %}
      ({{ i }},{{ j }})
    {% endfor %}
  {% endfor %}
LIQUID

CASE_STATEMENT = <<~LIQUID
  {% case type %}
    {% when 'a' %}A{% when 'b' %}B{% when 'c' %}C{% when 'd' %}D{% else %}Other
  {% endcase %}
LIQUID

COMPLEX_TEMPLATE = <<~LIQUID
  {% assign items = collection.products %}
  {% for product in items limit:10 %}
    {% if product.available %}
      <div class="product">
        <h2>{{ product.title | escape }}</h2>
        <p>{{ product.description | truncate: 100 }}</p>
        <span class="price">{{ product.price | money }}</span>
        {% if product.on_sale %}
          <span class="sale">On Sale!</span>
        {% endif %}
        {% for variant in product.variants %}
          {% case variant.type %}
            {% when 'size' %}
              <select>{% for size in variant.options %}
                <option>{{ size }}</option>
              {% endfor %}</select>
            {% when 'color' %}
              <div class="colors">{% for color in variant.options %}
                <span style="background:{{ color }}"></span>
              {% endfor %}</div>
          {% endcase %}
        {% endfor %}
      </div>
    {% endif %}
  {% endfor %}
LIQUID

MANY_VARIABLES = (1..50).map { |i| "{{ var#{i} }}" }.join

MANY_FILTERS = "{{ text | downcase | upcase | capitalize | strip | escape | truncate: 100 | prepend: 'pre' | append: 'post' }}"

SHOPIFY_LIKE_TEMPLATE = <<~LIQUID
  <!DOCTYPE html>
  <html>
  <head>
    <title>{{ shop.name }}</title>
  </head>
  <body>
    <header>
      <nav>
        {% for link in menu_links %}
          <a href="{{ link.url }}">{{ link.title }}</a>
        {% endfor %}
      </nav>
    </header>

    <main>
      {% if template == 'index' %}
        <h1>Welcome to {{ shop.name }}</h1>
        {% for product in featured_products limit:4 %}
          <div class="product-card">{{ product.title }}</div>
        {% endfor %}
      {% elsif template == 'product' %}
        <h1>{{ product.title }}</h1>
        <p>{{ product.description }}</p>
        <div class="variants">
          {% for variant in product.variants %}
            <option value="{{ variant.id }}">{{ variant.title }} - {{ variant.price }}</option>
          {% endfor %}
          <button type="submit">Add to Cart</button>
        </div>
      {% elsif template == 'collection' %}
        <h1>{{ collection.title }}</h1>
        {% for product in collection.products limit:12 %}
          <div class="product-card">{{ product.title }}</div>
        {% endfor %}
      {% endif %}
    </main>

    <footer>
      <p>{{ shop.name }}</p>
    </footer>
  </body>
  </html>
LIQUID

#-------------------------------------------------------------------------------
# Parsing Benchmarks
#-------------------------------------------------------------------------------

puts "\n=== PARSING BENCHMARKS ===\n\n"

Benchmark.ips do |x|
  x.report("parse: simple if") do
    Liquid::Template.parse(SIMPLE_IF)
  end

  x.report("parse: nested if") do
    Liquid::Template.parse(NESTED_IF)
  end

  x.report("parse: simple for") do
    Liquid::Template.parse(SIMPLE_FOR)
  end

  x.report("parse: nested for") do
    Liquid::Template.parse(NESTED_FOR)
  end

  x.report("parse: case statement") do
    Liquid::Template.parse(CASE_STATEMENT)
  end

  x.report("parse: complex template") do
    Liquid::Template.parse(COMPLEX_TEMPLATE)
  end

  x.report("parse: many variables") do
    Liquid::Template.parse(MANY_VARIABLES)
  end

  x.report("parse: many filters") do
    Liquid::Template.parse(MANY_FILTERS)
  end

  x.report("parse: shopify-like") do
    Liquid::Template.parse(SHOPIFY_LIKE_TEMPLATE)
  end

  x.compare!
end

#-------------------------------------------------------------------------------
# Combined Parse + Render Benchmarks
#-------------------------------------------------------------------------------

puts "\n=== PARSE + RENDER BENCHMARKS ===\n\n"

simple_context = { "condition" => true }
nested_context = { "a" => true, "b" => true, "c" => true, "d" => true }
case_context = { "type" => "b" }

Benchmark.ips do |x|
  x.report("parse+render: simple if") do
    Liquid::Template.parse(SIMPLE_IF).render!(simple_context)
  end

  x.report("parse+render: nested if") do
    Liquid::Template.parse(NESTED_IF).render!(nested_context)
  end

  x.report("parse+render: simple for") do
    Liquid::Template.parse(SIMPLE_FOR).render!
  end

  x.report("parse+render: nested for") do
    Liquid::Template.parse(NESTED_FOR).render!
  end

  x.report("parse+render: case") do
    Liquid::Template.parse(CASE_STATEMENT).render!(case_context)
  end

  x.compare!
end

#-------------------------------------------------------------------------------
# Render-only Benchmarks (pre-parsed templates)
#-------------------------------------------------------------------------------

puts "\n=== RENDER-ONLY BENCHMARKS (pre-parsed) ===\n\n"

simple_if_template = Liquid::Template.parse(SIMPLE_IF)
nested_if_template = Liquid::Template.parse(NESTED_IF)
simple_for_template = Liquid::Template.parse(SIMPLE_FOR)
nested_for_template = Liquid::Template.parse(NESTED_FOR)
case_template = Liquid::Template.parse(CASE_STATEMENT)

Benchmark.ips do |x|
  x.report("render: simple if") do
    simple_if_template.render!(simple_context)
  end

  x.report("render: nested if") do
    nested_if_template.render!(nested_context)
  end

  x.report("render: simple for") do
    simple_for_template.render!
  end

  x.report("render: nested for") do
    nested_for_template.render!
  end

  x.report("render: case") do
    case_template.render!(case_context)
  end

  x.compare!
end

#-------------------------------------------------------------------------------
# Memory Benchmark
#-------------------------------------------------------------------------------

puts "\n=== MEMORY USAGE ===\n\n"

def measure_memory
  GC.start
  GC.start
  before = GC.stat[:heap_live_slots]
  yield
  GC.start
  GC.start
  after = GC.stat[:heap_live_slots]
  after - before
end

templates_to_measure = {
  "simple if" => SIMPLE_IF,
  "nested if" => NESTED_IF,
  "simple for" => SIMPLE_FOR,
  "nested for" => NESTED_FOR,
  "case" => CASE_STATEMENT,
  "complex" => COMPLEX_TEMPLATE,
  "shopify-like" => SHOPIFY_LIKE_TEMPLATE,
}

templates_to_measure.each do |name, source|
  slots = measure_memory do
    100.times { Liquid::Template.parse(source) }
  end
  puts "#{name}: #{slots / 100} heap slots per parse (avg of 100)"
end

puts "\nBenchmark complete!"
