require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

module Liquid
  module C
    @enabled = true

    class << self
      attr_accessor :enabled
    end
  end
end

Liquid::Template.class_eval do
  private

  alias_method :ruby_tokenize, :tokenize

  def tokenize(source)
    if Liquid::C.enabled && !@line_numbers
      Liquid::Tokenizer.new(source.to_s)
    else
      ruby_tokenize(source)
    end
  end
end

Liquid::Variable.class_eval do
  alias_method :ruby_lax_parse, :lax_parse
  alias_method :ruby_strict_parse, :strict_parse

  def lax_parse(markup)
    stats = @options[:stats_callbacks]
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
