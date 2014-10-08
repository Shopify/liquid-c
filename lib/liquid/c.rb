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

