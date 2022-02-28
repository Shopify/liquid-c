require "bundler/setup"
require "liquid/c"

@template = Liquid::Template.parse(
"
Hi Frank

{% if name == 'match' %}
    HEY
    {% if name == 'frank' %}
        1 if frank
    {% elsif name == 'match' %}
        1 if match
    {% endif -%}
{% else %}
    1 Inside Else
{% endif -%}
{% if name == 'match' %}
    2 Inside if
{% else %}
    2 Inside Else
{%- endif %}

Hi Frank
", line_numbers: false)
puts @template.render({'name' => 'match'})
puts @template.root.body.disassemble

# Hi Tobi
# Hi Tobi
# {%- if name == 'tobi' -%}
# {%- if name == 'tobi' %}
# Inside 1
# {%- endif -%}
# {%- else %}
# Hi Frank
# {%- endif -%}
# {%- if name == 'tobi' -%}
# {%- if name == 'tobi' %}
# Inside 2
# {%- endif -%}
# {%- else %}
# Hi Frank
# {%- endif -%}