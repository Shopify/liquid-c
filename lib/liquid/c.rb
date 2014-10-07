require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::Template.class_eval do
  private

  def tokenize(source)
    Liquid::Tokenizer.new(source.to_s, :line_numbers => @line_numbers)
  end
end

Liquid::Variable.class_eval do
  private

  def lax_parse(markup)
    parser = Liquid::VariableParse.new(markup)
    @name = parser.name
    @filters = parser.filters
  end
end
