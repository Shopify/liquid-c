# frozen_string_literal: true

require "liquid/c/version"
require "liquid"

# Define Blank and Empty before loading C extension since parser.c needs them during Init
module Liquid
  module C
    # Blank singleton for blank keyword comparisons.
    # When compared with ==, checks if the other value is "blank".
    # Blank values: nil, false, empty strings, whitespace-only strings,
    # empty arrays, and empty hashes.
    class Blank
      INSTANCE = new.freeze

      class << self
        private :new
      end

      def ==(other)
        if other.respond_to?(:blank?)
          other.blank?
        else
          nil
        end
      end

      def to_s
        ""
      end

      # Used by variable_lookup_key when blank is used as a hash key
      def to_liquid_value
        ""
      end

      def inspect
        "Liquid::C::Blank"
      end
    end

    # Empty singleton for empty keyword comparisons.
    # When compared with ==, checks if the other value is "empty".
    # Empty values: empty strings, empty arrays, and empty hashes.
    # Note: nil and false are NOT empty (unlike blank).
    class Empty
      INSTANCE = new.freeze

      class << self
        private :new
      end

      def ==(other)
        if other.respond_to?(:empty?)
          other.empty?
        else
          nil
        end
      end

      def to_s
        ""
      end

      # Used by variable_lookup_key when empty is used as a hash key
      def to_liquid_value
        ""
      end

      def inspect
        "Liquid::C::Empty"
      end
    end
  end
end

require "liquid_c"
require "liquid/c/compile_ext"

Liquid::C::BlockBody.class_eval do
  def render(context)
    render_to_output_buffer(context, +"")
  end

  # Try native parsing using template_parser + codegen for the entire template.
  # Returns true if successful, false if should fall back to normal parsing.
  def try_native_parse(tokenizer, parse_context)
    return false unless Liquid::C.native_parse_enabled
    return false unless tokenizer.is_a?(Liquid::C::Tokenizer)

    # Try native parsing - returns true on success, false on failure
    parse_native(tokenizer, parse_context)
  rescue => e
    # On any error, fall back to normal parsing
    false
  end
end

module Liquid
  BlockBody.class_eval do
    class << self
      def c_rescue_render_node(context, output, line_number, exc, blank_tag)
        # There seems to be a MRI ruby bug with how the rb_rescue C function,
        # where $! gets set for its rescue callback, but it doesn't stay set
        # after a nested exception is raised and handled as is the case in
        # Liquid::Context#internal_error. This isn't a problem for plain ruby code,
        # so use a ruby rescue block to have setup $! properly.
        raise(exc)
      rescue => exc
        rescue_render_node(context, output, line_number, exc, blank_tag)
      end
    end
  end
end

module Liquid
  module C
    # Placeholder for variables in the Liquid::C::BlockBody#nodelist.
    class VariablePlaceholder
      class << self
        private :new
      end
    end

    class Tokenizer
      MAX_SOURCE_BYTE_SIZE = (1 << 24) - 1
    end

    # Enable native parsing using template_parser + codegen for full templates.
    # This provides better performance by parsing entire templates in C.
    class << self
      attr_accessor :native_parse_enabled
    end
    self.native_parse_enabled = false
  end
end

Liquid::Raw.class_eval do
  alias_method :ruby_parse, :parse

  def parse(tokenizer)
    if parse_context.liquid_c_nodes_disabled?
      ruby_parse(tokenizer)
    else
      c_parse(tokenizer)
    end
  end
end

Liquid::ParseContext.class_eval do
  class << self
    attr_accessor :liquid_c_nodes_disabled
  end
  self.liquid_c_nodes_disabled = false

  alias_method :ruby_new_block_body, :new_block_body

  def new_block_body
    if liquid_c_nodes_disabled?
      ruby_new_block_body
    else
      Liquid::C::BlockBody.new(self)
    end
  end

  alias_method :ruby_new_tokenizer, :new_tokenizer
  def new_tokenizer(source, start_line_number: nil, for_liquid_tag: false)
    unless liquid_c_nodes_disabled?
      source = source.to_s.to_str
      if source.bytesize <= Liquid::C::Tokenizer::MAX_SOURCE_BYTE_SIZE
        source = source.encode(Encoding::UTF_8)
        return Liquid::C::Tokenizer.new(source, start_line_number || 0, for_liquid_tag)
      else
        @liquid_c_nodes_disabled = true
      end
    end

    ruby_new_tokenizer(source, start_line_number: start_line_number, for_liquid_tag: for_liquid_tag)
  end

  def parse_expression(markup)
    if liquid_c_nodes_disabled?
      Liquid::Expression.parse(markup)
    else
      Liquid::C::Expression.lax_parse(markup)
    end
  end

  # @api private
  def liquid_c_nodes_disabled?
    # Liquid::Profiler exposes the internal parse tree that we don't want to build when
    # parsing with liquid-c, so consider liquid-c to be disabled when using it.
    # Also, some templates are parsed before the profiler is running, on which case we
    # provide the `disable_liquid_c_nodes` option to enable the Ruby AST to be produced
    # so the profiler can use it on future runs.
    return @liquid_c_nodes_disabled if defined?(@liquid_c_nodes_disabled)

    @liquid_c_nodes_disabled = !Liquid::C.enabled || @template_options[:profile] ||
      @template_options[:disable_liquid_c_nodes] || self.class.liquid_c_nodes_disabled
  end
end

module Liquid
  module C
    module DocumentClassPatch
      def parse(tokenizer, parse_context)
        if tokenizer.is_a?(Liquid::C::Tokenizer)
          if parse_context[:bug_compatible_whitespace_trimming]
            # Temporary to test rollout of the fix for this bug
            tokenizer.bug_compatible_whitespace_trimming!
          end

          begin
            parse_context.start_liquid_c_parsing
            super
          ensure
            parse_context.cleanup_liquid_c_parsing
          end
        else
          super
        end
      end
    end
    Liquid::Document.singleton_class.prepend(DocumentClassPatch)

    # Patch the instance method to try native parsing
    module DocumentInstancePatch
      def parse(tokenizer, parse_context)
        if Liquid::C.native_parse_enabled &&
           tokenizer.is_a?(Liquid::C::Tokenizer) &&
           @body.is_a?(Liquid::C::BlockBody)
          # Try native parsing - parses entire template in C
          if @body.try_native_parse(tokenizer, parse_context)
            @body.freeze
            return
          end
          # Native parsing failed, fall through to normal parsing
        end
        super
      end
    end
    Liquid::Document.prepend(DocumentInstancePatch)
  end
end

Liquid::Variable.class_eval do
  class << self
    # @api private
    def call_variable_fallback_stats_callback(parse_context)
      callbacks = parse_context[:stats_callbacks]
      callbacks[:variable_fallback]&.call if callbacks
    end

    private

    # helper method for C code
    def rescue_strict_parse_syntax_error(error, markup, parse_context)
      error.line_number = parse_context.line_number
      error.markup_context = "in \"{{#{markup}}}\""
      case parse_context.error_mode
      when :strict
        raise
      when :warn
        parse_context.warnings << error
      end
      call_variable_fallback_stats_callback(parse_context)
      lax_parse(markup, parse_context) # Avoid redundant strict parse
    end

    def lax_parse(markup, parse_context)
      old_error_mode = parse_context.error_mode
      begin
        set_error_mode(parse_context, :lax)
        new(markup, parse_context)
      ensure
        set_error_mode(parse_context, old_error_mode)
      end
    end

    def set_error_mode(parse_context, mode)
      parse_context.instance_variable_set(:@error_mode, mode)
    end
  end

  alias_method :ruby_strict_parse, :strict_parse

  def strict_parse(markup)
    if parse_context.liquid_c_nodes_disabled?
      ruby_strict_parse(markup)
    else
      c_strict_parse(markup)
    end
  end
end

Liquid::StrainerTemplate.class_eval do
  class << self
    private

    def filter_methods_hash
      @filter_methods_hash ||= {}.tap do |hash|
        filter_methods.each do |method_name|
          hash[method_name.to_sym] = true
        end
      end
    end

    # Convert wrong number of argument error into a liquid exception to
    # treat it as an error in the template, not an internal error.
    def arg_exc_to_liquid_exc(argument_error)
      raise Liquid::ArgumentError, argument_error.message, argument_error.backtrace
    rescue Liquid::ArgumentError => liquid_exc
      liquid_exc
    end
  end
end

Liquid::C::Expression.class_eval do
  class << self
    def lax_parse(markup)
      strict_parse(markup)
    rescue Liquid::SyntaxError
      Liquid::Expression.parse(markup)
    end

    # Default to strict parsing, since Liquid::C::Expression.parse should only really
    # be used with constant expressions.  Otherwise, prefer parse_context.parse_expression.
    alias_method :parse, :strict_parse
  end
end

Liquid::Context.class_eval do
  alias_method :ruby_parse_evaluate, :[]
  alias_method :ruby_evaluate, :evaluate
  alias_method :ruby_find_variable, :find_variable
  alias_method :ruby_strict_variables=, :strict_variables=

  # This isn't entered often by Ruby (most calls stay in C via VariableLookup#evaluate)
  # so the wrapper method isn't costly.
  def c_find_variable_kwarg(key, raise_on_not_found: true)
    c_find_variable(key, raise_on_not_found)
  end

  def c_parse_evaluate(expression)
    c_evaluate(Liquid::C::Expression.lax_parse(expression))
  end
end

Liquid::ResourceLimits.class_eval do
  class << self
    def new(limits)
      if Liquid::C.enabled
        Liquid::C::ResourceLimits.new(
          limits[:render_length_limit],
          limits[:render_score_limit],
          limits[:assign_score_limit],
        )
      else
        super
      end
    end
  end
end

module Liquid
  module C
    class << self
      attr_reader :enabled

      def enabled=(value)
        @enabled = value
        if value
          Liquid::Context.send(:alias_method, :[], :c_parse_evaluate)
          Liquid::Context.send(:alias_method, :evaluate, :c_evaluate)
          Liquid::Context.send(:alias_method, :find_variable, :c_find_variable_kwarg)
          Liquid::Context.send(:alias_method, :strict_variables=, :c_strict_variables=)
        else
          Liquid::Context.send(:alias_method, :[], :ruby_parse_evaluate)
          Liquid::Context.send(:alias_method, :evaluate, :ruby_evaluate)
          Liquid::Context.send(:alias_method, :find_variable, :ruby_find_variable)
          Liquid::Context.send(:alias_method, :strict_variables=, :ruby_strict_variables=)
        end
      end
    end

    self.enabled = true
  end
end
