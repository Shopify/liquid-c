# frozen_string_literal: true
source 'https://rubygems.org'

gemspec

gem 'liquid', github: 'Shopify/liquid', ref: 'master'

group :test do
  gem 'rubocop', '~> 0.93.1', require: false
  gem 'rubocop-performance', '~> 1.8.1', require: false
  gem 'rubocop-shopify', '~> 1.0.6', require: false
  gem 'spy', '0.4.1'
  gem 'benchmark-ips'
end

group :development do
  gem 'byebug'
end
