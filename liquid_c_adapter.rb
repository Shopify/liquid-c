# frozen_string_literal: true

# Liquid-C Spec Adapter
#
# This adapter allows running liquid-spec conformance tests against liquid-c.
# Run with: liquid-spec liquid_c_adapter.rb

require "liquid/spec/cli/adapter_dsl"

LiquidSpec.setup do |ctx|
  # Add the lib path for liquid-c
  $LOAD_PATH.unshift(File.expand_path("lib", __dir__))

  # Compile the C extension if needed
  ext_path = File.expand_path("lib/liquid_c.bundle", __dir__)
  unless File.exist?(ext_path)
    system("bundle exec rake compile") or raise "Failed to compile liquid-c"
  end

  require "liquid/c"

  # liquid-spec expects Liquid::Environment.default.register_filter to exist.
  # Provide a minimal shim for older Liquid versions.
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
  parse_options[:error_mode] = options[:error_mode] if options.key?(:error_mode)

  ctx[:template] = Liquid::Template.parse(source, **parse_options)
end

# Called to render a compiled template with the given context.
LiquidSpec.render do |ctx, assigns, options|
  options ||= {}
  template = ctx[:template]
  registers = options[:registers] || {}

  context = Liquid::Context.build(
    environments: [assigns],
    registers: Liquid::Registers.new(registers)
  )

  if options[:strict_errors]
    context.strict_variables = true
  end

  template.render(context)
end
