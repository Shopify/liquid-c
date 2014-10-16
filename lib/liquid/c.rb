require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::Template.class_eval do
  private

  alias_method :ruby_tokenize, :tokenize

  def tokenize(source)
    if @line_numbers
      ruby_tokenize(source)
    else
      Liquid::Tokenizer.new(source.to_s)
    end
  end
end

Liquid::Lexer.class_eval do
  def initialize(input)
    @input = input
  end

  def tokenize
    Liquid::Lexer.c_lex(@input)
  end
end

Liquid::Variable.class_eval do
  alias_method :ruby_lax_parse, :lax_parse

  def lax_parse(markup)
    begin
      strict_parse(markup)
    rescue
      ruby_lax_parse(markup)
    end
  end

  def strict_parse(markup)
    @filters = []
    @name = Liquid::Variable.c_strict_parse(markup, @filters)
  end
end
