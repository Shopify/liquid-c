# frozen_string_literal: true

require "test_helper"

# Integration tests with complex real-world template patterns
class TemplateParserIntegrationTest < Minitest::Test
  #-----------------------------------------------------------------------------
  # E-commerce Templates
  #-----------------------------------------------------------------------------

  def test_product_listing_template
    source = <<~LIQUID
      {% for product in products %}
        <div class="product">
          <h2>{{ product.title | escape }}</h2>
          <p>{{ product.description | truncate: 100 }}</p>
          {% if product.on_sale %}
            <span class="original-price">{{ product.compare_at_price | money }}</span>
            <span class="sale-price">{{ product.price | money }}</span>
          {% else %}
            <span class="price">{{ product.price | money }}</span>
          {% endif %}
          {% if product.variants.size > 1 %}
            <select name="variant">
              {% for variant in product.variants %}
                <option value="{{ variant.id }}">{{ variant.title }}</option>
              {% endfor %}
            </select>
          {% endif %}
        </div>
      {% endfor %}
    LIQUID

    template = Liquid::Template.parse(source)
    products = [
      {
        "title" => "Widget",
        "description" => "A great widget",
        "on_sale" => true,
        "compare_at_price" => 100,
        "price" => 80,
        "variants" => [
          { "id" => 1, "title" => "Small" },
          { "id" => 2, "title" => "Large" },
        ],
      },
      {
        "title" => "Gadget",
        "description" => "A useful gadget",
        "on_sale" => false,
        "price" => 50,
        "variants" => [{ "id" => 3, "title" => "Default" }],
      },
    ]

    output = template.render!({ "products" => products })
    assert_includes(output, "Widget")
    assert_includes(output, "sale-price")
    assert_includes(output, "Gadget")
    assert_includes(output, "Small")
    assert_includes(output, "Large")
  end

  def test_cart_template
    source = <<~LIQUID
      {% if cart.items.size > 0 %}
        <table>
          {% for item in cart.items %}
            <tr>
              <td>{{ item.product.title }}</td>
              <td>{{ item.quantity }}</td>
              <td>{{ item.line_price | money }}</td>
            </tr>
          {% endfor %}
        </table>
        <div class="total">
          {% assign total = 0 %}
          {% for item in cart.items %}
            {% assign total = total | plus: item.line_price %}
          {% endfor %}
          Total: {{ total | money }}
        </div>
      {% else %}
        <p>Your cart is empty.</p>
      {% endif %}
    LIQUID

    template = Liquid::Template.parse(source)

    empty_cart = { "cart" => { "items" => [] } }
    assert_includes(template.render!(empty_cart), "cart is empty")

    full_cart = {
      "cart" => {
        "items" => [
          { "product" => { "title" => "Item A" }, "quantity" => 2, "line_price" => 100 },
          { "product" => { "title" => "Item B" }, "quantity" => 1, "line_price" => 50 },
        ],
      },
    }
    output = template.render!(full_cart)
    assert_includes(output, "Item A")
    assert_includes(output, "Item B")
    assert_includes(output, "Total")
  end

  #-----------------------------------------------------------------------------
  # Navigation Templates
  #-----------------------------------------------------------------------------

  def test_nested_navigation_template
    source = <<~LIQUID
      <nav>
        <ul>
          {% for link in navigation.links %}
            <li>
              <a href="{{ link.url }}">{{ link.title }}</a>
              {% if link.children.size > 0 %}
                <ul class="submenu">
                  {% for child in link.children %}
                    <li>
                      <a href="{{ child.url }}">{{ child.title }}</a>
                      {% if child.children.size > 0 %}
                        <ul class="sub-submenu">
                          {% for grandchild in child.children %}
                            <li><a href="{{ grandchild.url }}">{{ grandchild.title }}</a></li>
                          {% endfor %}
                        </ul>
                      {% endif %}
                    </li>
                  {% endfor %}
                </ul>
              {% endif %}
            </li>
          {% endfor %}
        </ul>
      </nav>
    LIQUID

    template = Liquid::Template.parse(source)
    navigation = {
      "navigation" => {
        "links" => [
          {
            "title" => "Home",
            "url" => "/",
            "children" => [],
          },
          {
            "title" => "Products",
            "url" => "/products",
            "children" => [
              {
                "title" => "Category A",
                "url" => "/products/a",
                "children" => [
                  { "title" => "Sub A1", "url" => "/products/a/1", "children" => [] },
                ],
              },
            ],
          },
        ],
      },
    }

    output = template.render!(navigation)
    assert_includes(output, "Home")
    assert_includes(output, "Products")
    assert_includes(output, "Category A")
    assert_includes(output, "Sub A1")
    assert_includes(output, "submenu")
    assert_includes(output, "sub-submenu")
  end

  #-----------------------------------------------------------------------------
  # Conditional Display Templates
  #-----------------------------------------------------------------------------

  def test_user_role_template
    source = <<~LIQUID
      {% case user.role %}
        {% when 'admin' %}
          <div class="admin-panel">
            <h1>Admin Dashboard</h1>
            {% if user.permissions.can_manage_users %}
              <a href="/users">Manage Users</a>
            {% endif %}
            {% if user.permissions.can_view_reports %}
              <a href="/reports">View Reports</a>
            {% endif %}
          </div>
        {% when 'moderator' %}
          <div class="mod-panel">
            <h1>Moderator Tools</h1>
            {% for tool in moderator_tools %}
              <a href="{{ tool.url }}">{{ tool.name }}</a>
            {% endfor %}
          </div>
        {% when 'user' %}
          <div class="user-panel">
            <h1>Welcome, {{ user.name }}</h1>
          </div>
        {% else %}
          <div class="guest-panel">
            <a href="/login">Please log in</a>
          </div>
      {% endcase %}
    LIQUID

    template = Liquid::Template.parse(source)

    admin = {
      "user" => {
        "role" => "admin",
        "permissions" => { "can_manage_users" => true, "can_view_reports" => true },
      },
    }
    output = template.render!(admin)
    assert_includes(output, "Admin Dashboard")
    assert_includes(output, "Manage Users")
    assert_includes(output, "View Reports")

    moderator = {
      "user" => { "role" => "moderator" },
      "moderator_tools" => [{ "name" => "Ban User", "url" => "/mod/ban" }],
    }
    output = template.render!(moderator)
    assert_includes(output, "Moderator Tools")
    assert_includes(output, "Ban User")

    guest = {}
    output = template.render!(guest)
    assert_includes(output, "Please log in")
  end

  #-----------------------------------------------------------------------------
  # Table Generation
  #-----------------------------------------------------------------------------

  def test_data_table_template
    source = <<~LIQUID
      <table>
        <thead>
          <tr>
            {% for header in headers %}
              <th>{{ header }}</th>
            {% endfor %}
          </tr>
        </thead>
        <tbody>
          {% tablerow row in rows cols:headers.size %}
            {% for cell in row %}
              {{ cell }}
            {% endfor %}
          {% endtablerow %}
        </tbody>
      </table>
    LIQUID

    template = Liquid::Template.parse(source)
    data = {
      "headers" => %w[Name Age City],
      "rows" => [
        %w[Alice 30 NYC],
        %w[Bob 25 LA],
        %w[Charlie 35 Chicago],
      ],
    }

    output = template.render!(data)
    assert_includes(output, "<th>Name</th>")
    assert_includes(output, "Alice")
    assert_includes(output, "Bob")
    assert_includes(output, "Chicago")
  end

  #-----------------------------------------------------------------------------
  # Variable Manipulation
  #-----------------------------------------------------------------------------

  def test_complex_variable_manipulation
    source = <<~LIQUID
      {% assign all_tags = "" %}
      {% for product in products %}
        {% for tag in product.tags %}
          {% unless all_tags contains tag %}
            {% if all_tags != "" %}
              {% assign all_tags = all_tags | append: "," %}
            {% endif %}
            {% assign all_tags = all_tags | append: tag %}
          {% endunless %}
        {% endfor %}
      {% endfor %}
      {% assign unique_tags = all_tags | split: "," %}
      Tags: {% for tag in unique_tags %}{{ tag }}{% unless forloop.last %}, {% endunless %}{% endfor %}
    LIQUID

    template = Liquid::Template.parse(source)
    products = {
      "products" => [
        { "tags" => %w[sale new] },
        { "tags" => %w[featured sale] },
        { "tags" => %w[new limited] },
      ],
    }

    output = template.render!(products)
    assert_includes(output, "sale")
    assert_includes(output, "new")
    assert_includes(output, "featured")
    assert_includes(output, "limited")
  end

  def test_capture_with_conditionals
    source = <<~LIQUID
      {% capture greeting %}
        {% if time_of_day == "morning" %}
          Good morning
        {% elsif time_of_day == "afternoon" %}
          Good afternoon
        {% elsif time_of_day == "evening" %}
          Good evening
        {% else %}
          Hello
        {% endif %}
      {% endcapture %}
      {{ greeting | strip }}, {{ user.name }}!
    LIQUID

    template = Liquid::Template.parse(source)

    morning = { "time_of_day" => "morning", "user" => { "name" => "Alice" } }
    assert_includes(template.render!(morning).strip, "Good morning")
    assert_includes(template.render!(morning).strip, "Alice")

    afternoon = { "time_of_day" => "afternoon", "user" => { "name" => "Bob" } }
    assert_includes(template.render!(afternoon).strip, "Good afternoon")
  end

  #-----------------------------------------------------------------------------
  # Pagination Pattern
  #-----------------------------------------------------------------------------

  def test_pagination_template
    source = <<~LIQUID
      {% assign page_size = 3 %}
      {% assign total_pages = items.size | divided_by: page_size %}
      {% if items.size | modulo: page_size > 0 %}
        {% assign total_pages = total_pages | plus: 1 %}
      {% endif %}

      {% assign start = current_page | minus: 1 | times: page_size %}
      {% assign end = start | plus: page_size | minus: 1 %}

      <ul>
      {% for item in items limit:page_size offset:start %}
        <li>{{ item }}</li>
      {% endfor %}
      </ul>

      <nav>
        {% if current_page > 1 %}
          <a href="?page={{ current_page | minus: 1 }}">Previous</a>
        {% endif %}
        {% for i in (1..total_pages) %}
          {% if i == current_page %}
            <span class="current">{{ i }}</span>
          {% else %}
            <a href="?page={{ i }}">{{ i }}</a>
          {% endif %}
        {% endfor %}
        {% if current_page < total_pages %}
          <a href="?page={{ current_page | plus: 1 }}">Next</a>
        {% endif %}
      </nav>
    LIQUID

    template = Liquid::Template.parse(source)
    data = {
      "items" => %w[A B C D E F G H I],
      "current_page" => 2,
    }

    output = template.render!(data)
    assert_includes(output, "<li>D</li>")
    assert_includes(output, "<li>E</li>")
    assert_includes(output, "<li>F</li>")
    assert_includes(output, "Previous")
    assert_includes(output, "Next")
  end

  #-----------------------------------------------------------------------------
  # Forloop Variables
  #-----------------------------------------------------------------------------

  def test_forloop_all_variables
    source = <<~LIQUID
      {% for item in items %}
        index: {{ forloop.index }}
        index0: {{ forloop.index0 }}
        rindex: {{ forloop.rindex }}
        rindex0: {{ forloop.rindex0 }}
        first: {{ forloop.first }}
        last: {{ forloop.last }}
        length: {{ forloop.length }}
        ---
      {% endfor %}
    LIQUID

    template = Liquid::Template.parse(source)
    output = template.render!({ "items" => %w[a b c] })

    # First item
    assert_includes(output, "index: 1")
    assert_includes(output, "index0: 0")
    assert_includes(output, "first: true")

    # Last item
    assert_includes(output, "index: 3")
    assert_includes(output, "last: true")
    assert_includes(output, "rindex: 1")
    assert_includes(output, "rindex0: 0")

    # Length
    assert_includes(output, "length: 3")
  end

  def test_nested_forloop_with_parentloop
    source = <<~LIQUID
      {% for outer in (1..2) %}
        outer.index: {{ forloop.index }}
        {% for inner in (1..2) %}
          inner.index: {{ forloop.index }}
          parentloop.index: {{ forloop.parentloop.index }}
        {% endfor %}
      {% endfor %}
    LIQUID

    template = Liquid::Template.parse(source)
    output = template.render!

    assert_includes(output, "outer.index: 1")
    assert_includes(output, "outer.index: 2")
    assert_includes(output, "inner.index: 1")
    assert_includes(output, "parentloop.index: 1")
    assert_includes(output, "parentloop.index: 2")
  end

  #-----------------------------------------------------------------------------
  # Edge Cases
  #-----------------------------------------------------------------------------

  def test_empty_blocks_render_correctly
    source = <<~LIQUID
      {% if true %}{% endif %}{% for i in (1..0) %}{% endfor %}{% case x %}{% endcase %}
    LIQUID
    template = Liquid::Template.parse(source)
    assert_equal("", template.render!.strip)
  end

  def test_mixed_content_and_tags
    source = "Hello {% if true %}beautiful{% endif %} world{{ '!' }}"
    template = Liquid::Template.parse(source)
    assert_equal("Hello beautiful world!", template.render!)
  end

  def test_unicode_in_templates
    source = <<~LIQUID
      {% assign greeting = "Hallo" %}
      {{ greeting }}, {{ name }}! Today is {{ day }}.
    LIQUID

    template = Liquid::Template.parse(source)
    output = template.render!({ "name" => "Muller", "day" => "Montag" })
    assert_includes(output, "Hallo")
    assert_includes(output, "Muller")
  end

  def test_special_characters_in_strings
    # Liquid doesn't support escape sequences in strings, so we use single quotes
    source = "{% assign msg = '<>&' %}{{ msg | escape }}"
    template = Liquid::Template.parse(source)
    output = template.render!
    assert_includes(output, "&lt;")
    assert_includes(output, "&gt;")
    assert_includes(output, "&amp;")
  end
end
