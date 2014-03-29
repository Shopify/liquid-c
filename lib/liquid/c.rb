require 'liquid/c/version'
require 'liquid'

# workaround bundler issue #2947 which could cause an old extension to be used
require 'rbconfig'
ext_path = File.expand_path("../../liquid_c.#{RbConfig::CONFIG['DLEXT']}", __FILE__)
if File.exists?(ext_path)
  require ext_path
else
  require 'liquid_c'
end

Liquid::Template.class_eval do
  private

  def tokenize(source)
    Liquid::Tokenizer.new(source.to_s)
  end
end
