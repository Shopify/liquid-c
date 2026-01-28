# frozen_string_literal: true

# Liquid-C Spec Adapter
#
# This adapter allows running liquid-spec conformance tests against liquid-c.
# Run with: liquid-spec liquid_c_adapter.rb

# Load liquid-c BEFORE requiring adapter_dsl, since liquid-spec's test_filters.rb
# expects Liquid::Environment to exist when it's loaded.
$LOAD_PATH.unshift(File.expand_path("lib", __dir__))

# Compile the C extension if needed
ext_path = File.expand_path("lib/liquid_c.bundle", __dir__)
unless File.exist?(ext_path)
  system("bundle exec rake compile") or raise "Failed to compile liquid-c"
end

require "liquid/c"

# liquid-spec expects Liquid::Environment.default.register_filter to exist.
# Provide a minimal shim for older Liquid versions that don't have it.
unless defined?(Liquid::Environment)
  module Liquid
    class Environment
      def self.default
        @default ||= new
      end

      def register_filter(mod)
        Liquid::Template.register_filter(mod)
      end
    end
  end
end

require "liquid/spec/cli/adapter_dsl"

LiquidSpec.setup do |ctx|
  # Nothing special needed here - liquid-c is already loaded
end

LiquidSpec.configure do |config|
  # Run the liquid_ruby suite which tests core Liquid functionality
  config.suite = :liquid_ruby
end

# Called to compile a template string into a Liquid template object.
LiquidSpec.compile do |ctx, source, options|
  options ||= {}
  parse_options = {}
  parse_options[:line_numbers] = options[:line_numbers] if options.key?(:line_numbers)
  # Default to lax mode unless strict is explicitly requested
  parse_options[:error_mode] = options[:error_mode] || :lax

  ctx[:template] = Liquid::Template.parse(source, **parse_options)
end

# Called to render a compiled template with the given context.
LiquidSpec.render do |ctx, assigns, options|
  options ||= {}
  template = ctx[:template]
  registers = options[:registers] || {}

  # strict_errors controls whether errors are raised as exceptions vs rendered inline.
  # strict_variables controls whether undefined variables raise exceptions.
  # These are separate concerns - liquid-spec's strict_errors should NOT enable strict_variables
  # because undefined variables returning nil is standard Liquid behavior.
  strict_errors = options[:strict_errors] == true

  context = Liquid::Context.build(
    static_environments: [assigns],
    registers: Liquid::Registers.new(registers),
    rethrow_errors: strict_errors
  )

  # Never enable strict_variables - undefined variables should return nil per Liquid spec
  context.strict_variables = false

  template.render(context)
end
