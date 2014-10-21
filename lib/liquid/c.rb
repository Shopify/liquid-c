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
    if Liquid::C.enabled
      begin
        return strict_parse(markup)
      rescue Liquid::SyntaxError
      end
    end

    ruby_lax_parse(markup)
  end

  def strict_parse(markup)
    if Liquid::C.enabled
      @name, @filters = Liquid::Variable.c_strict_parse(markup)
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
