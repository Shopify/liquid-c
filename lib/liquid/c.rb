# frozen_string_literal: true

require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::C::BlockBody.class_eval do
  def render(context)
    render_to_output_buffer(context, +'')
  end
end

module Liquid
  BlockBody.class_eval do
    def self.c_rescue_render_node(context, output, line_number, exc, blank_tag)
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

# Placeholder for variables in the Liquid::C::BlockBody#nodelist.
class Liquid::C::VariablePlaceholder
  class << self
    private :new
  end
end

Liquid::Tokenizer.class_eval do
  def self.new(source, line_numbers = false, line_number: nil, for_liquid_tag: false)
    if Liquid::C.enabled
      source = source.to_s.encode(Encoding::UTF_8)
      Liquid::C::Tokenizer.new(source, line_number || (line_numbers ? 1 : 0), for_liquid_tag)
    else
      super
    end
  end
end

Liquid::Raw.class_eval do
  alias_method :ruby_parse, :parse
end

Liquid::ParseContext.class_eval do
  alias_method :ruby_new_block_body, :new_block_body

  def new_block_body
    if disable_liquid_c_nodes
      ruby_new_block_body
    else
      Liquid::C::BlockBody.new
    end
  end

  private

  def disable_liquid_c_nodes
    # Liquid::Profiler exposes the internal parse tree that we don't want to build when
    # parsing with liquid-c, so consider liquid-c to be disabled when using it.
    @disable_liquid_c_nodes ||= !Liquid::C.enabled || @template_options[:profile]
  end
end

module Liquid::C
  # Temporary to test rollout of the fix for this bug
  module DocumentPatch
    def parse(
      tokenizer,
      parse_context = self.parse_context # no longer necessary, so allow the liquid gem to stop passing it in
    )
      if tokenizer.is_a?(Liquid::C::Tokenizer) && parse_context[:bug_compatible_whitespace_trimming]
        tokenizer.bug_compatible_whitespace_trimming!
      end
      super
    end
  end
  Liquid::Document.prepend(DocumentPatch)
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

  alias_method :ruby_lax_parse, :lax_parse
  alias_method :ruby_strict_parse, :strict_parse

  def lax_parse(markup)
    if Liquid::C.enabled
      begin
        return strict_parse(markup)
      rescue Liquid::SyntaxError
        Liquid::Variable.call_variable_fallback_stats_callback(parse_context)
      end
    end

    ruby_lax_parse(markup)
  end

  def strict_parse(markup)
    if Liquid::C.enabled
      @name = Liquid::Variable.c_strict_parse(markup, @filters = [])
    else
      ruby_strict_parse(markup)
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
  end
end

Liquid::VariableLookup.class_eval do
  alias_method :ruby_initialize, :initialize

  def initialize(markup, name = nil, lookups = nil, command_flags = nil)
    if Liquid::C.enabled && markup == false
      @name = name
      @lookups = lookups
      @command_flags = command_flags
    else
      ruby_initialize(markup)
    end
  end
end

Liquid::Expression.class_eval do
  class << self
    alias_method :ruby_parse, :parse

    def parse(markup)
      return nil unless markup

      if Liquid::C.enabled
        begin
          return c_parse(markup)
        rescue Liquid::SyntaxError
        end
      end
      ruby_parse(markup)
    end
  end
end

Liquid::Context.class_eval do
  alias_method :ruby_evaluate, :evaluate
  alias_method :ruby_find_variable, :find_variable

  # This isn't entered often by Ruby (most calls stay in C via VariableLookup#evaluate)
  # so the wrapper method isn't costly.
  def c_find_variable_kwarg(key, raise_on_not_found: true)
    c_find_variable(key, raise_on_not_found)
  end
end

Liquid::ResourceLimits.class_eval do
  def self.new(limits)
    if Liquid::C.enabled
      Liquid::C::ResourceLimits.new(
        limits[:render_length_limit],
        limits[:render_score_limit],
        limits[:assign_score_limit]
      )
    else
      super
    end
  end
end

Liquid::VariableLookup.class_eval do
  alias_method :ruby_evaluate, :evaluate
end

module Liquid
  module C
    class << self
      attr_reader :enabled

      def enabled=(value)
        @enabled = value
        if value
          Liquid::Context.send(:alias_method, :evaluate, :c_evaluate)
          Liquid::Context.send(:alias_method, :find_variable, :c_find_variable_kwarg)
          Liquid::Raw.send(:alias_method, :parse, :c_parse)
          Liquid::VariableLookup.send(:alias_method, :evaluate, :c_evaluate)
        else
          Liquid::Context.send(:alias_method, :evaluate, :ruby_evaluate)
          Liquid::Context.send(:alias_method, :find_variable, :ruby_find_variable)
          Liquid::Raw.send(:alias_method, :parse, :ruby_parse)
          Liquid::VariableLookup.send(:alias_method, :evaluate, :ruby_evaluate)
        end
      end
    end

    self.enabled = true
  end
end
