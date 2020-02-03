# frozen_string_literal: true

require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::Tokenizer.class_eval do
  def self.new(source, line_numbers = false, line_number: nil, for_liquid_tag: false)
    if Liquid::C.enabled
      Liquid::C::Tokenizer.new(source.to_s, line_number || (line_numbers ? 1 : 0), for_liquid_tag)
    else
      super
    end
  end
end

Liquid::BlockBody.class_eval do
  alias_method :ruby_parse, :parse

  def parse(tokens, options)
    if Liquid::C.enabled && !options[:profile]
      c_parse(tokens, options) { |t, m| yield t, m }
    else
      ruby_parse(tokens, options) { |t, m| yield t, m }
    end
  end
end

module Liquid::C
  # Temporary to test rollout of the fix for this bug
  module DocumentPatch
    def parse(tokenizer, parse_context)
      if tokenizer.is_a?(Liquid::C::Tokenizer) && parse_context[:bug_compatible_whitespace_trimming]
        tokenizer.bug_compatible_whitespace_trimming!
      end
      super
    end
  end
  Liquid::Document.prepend(DocumentPatch)
end

Liquid::Variable.class_eval do
  alias_method :ruby_lax_parse, :lax_parse
  alias_method :ruby_strict_parse, :strict_parse

  def lax_parse(markup)
    stats = options[:stats_callbacks]
    stats[:variable_parse].call if stats

    if Liquid::C.enabled
      begin
        return strict_parse(markup)
      rescue Liquid::SyntaxError
        stats[:variable_fallback].call if stats
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
          Liquid::VariableLookup.send(:alias_method, :evaluate, :c_evaluate)
        else
          Liquid::Context.send(:alias_method, :evaluate, :ruby_evaluate)
          Liquid::Context.send(:alias_method, :find_variable, :ruby_find_variable)
          Liquid::VariableLookup.send(:alias_method, :evaluate, :ruby_evaluate)
        end
      end
    end

    self.enabled = true
  end
end
