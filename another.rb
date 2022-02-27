
require 'rubygems'
require 'bundler/setup'
require "liquid/c"

@template = Liquid::Template.parse(
"
{%- if name == 'tobi' -%}
Hi Tobi
{%- else -%}
Hi Frank
{%- endif -%}
"
)
puts @template.render('name' => 'to')