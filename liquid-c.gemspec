# frozen_string_literal: true

lib = File.expand_path('lib', __dir__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'liquid/c/version'

Gem::Specification.new do |spec|
  spec.name          = 'liquid-c'
  spec.version       = Liquid::C::VERSION
  spec.authors       = ['Justin Li', 'Dylan Thacker-Smith']
  spec.email         = ['gems@shopify.com']
  spec.summary       = 'Liquid performance extension in C'
  spec.homepage      = 'https://github.com/shopify/liquid-c'
  spec.license       = 'MIT'

  spec.extensions    = ['ext/liquid_c/extconf.rb']
  spec.files         = `git ls-files -z`.split("\x0")
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})

  spec.require_paths = ['lib']
  spec.required_ruby_version = '>= 2.2.0'

  spec.add_dependency 'liquid', '>= 3.0.0'

  spec.add_development_dependency 'bundler', '~> 1.5'
  spec.add_development_dependency 'minitest'
  spec.add_development_dependency 'rake'
  spec.add_development_dependency 'rake-compiler'
  spec.add_development_dependency 'rubocop'
  spec.add_development_dependency 'stackprof'
end
