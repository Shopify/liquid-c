require "bundler/setup"
require "liquid/c"

@template = Liquid::Template.parse(
"
normal raw text

{% if name == 'match' %}
    text matched from if
    {% if name == 'frank' %}
        1 text matched from nested if
    {% elsif name == 'match' %}
        1 text matched from nested elsif
    {% endif -%}
{% else %}
    1 text matched from else
{% endif -%}
{% if name == 'frank' %}
    2 text matched from if
{% else %}
    2 text matched from else
{%- endif %}

normal raw text
", line_numbers: false)
puts @template.render({'name' => 'match'})
puts @template.root.body.disassemble
