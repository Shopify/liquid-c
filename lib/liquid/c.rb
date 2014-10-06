require 'liquid/c/version'
require 'liquid'
require 'liquid_c'

Liquid::Template.class_eval do
  private

  def tokenize(source)
    Liquid::Tokenizer.new(source.to_s)
  end
end
