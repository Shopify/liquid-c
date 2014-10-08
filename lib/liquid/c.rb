require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::Template.class_eval do
  private

  def tokenize(source)
    Liquid::Tokenizer.new(source.to_s, :line_numbers => @line_numbers)
  end
end

