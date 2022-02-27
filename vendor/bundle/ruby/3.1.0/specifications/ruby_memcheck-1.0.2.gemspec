# -*- encoding: utf-8 -*-
# stub: ruby_memcheck 1.0.2 ruby lib

Gem::Specification.new do |s|
  s.name = "ruby_memcheck".freeze
  s.version = "1.0.2"

  s.required_rubygems_version = Gem::Requirement.new(">= 0".freeze) if s.respond_to? :required_rubygems_version=
  s.metadata = { "homepage_uri" => "https://github.com/peterzhu2118/ruby_memcheck" } if s.respond_to? :metadata=
  s.require_paths = ["lib".freeze]
  s.authors = ["Peter Zhu".freeze]
  s.bindir = "exe".freeze
  s.date = "2021-11-04"
  s.email = ["peter@peterzhu.ca".freeze]
  s.homepage = "https://github.com/peterzhu2118/ruby_memcheck".freeze
  s.licenses = ["MIT".freeze]
  s.required_ruby_version = Gem::Requirement.new(">= 2.3.0".freeze)
  s.rubygems_version = "3.3.7".freeze
  s.summary = "Use Valgrind memcheck without going crazy".freeze

  s.installed_by_version = "3.3.7" if s.respond_to? :installed_by_version

  if s.respond_to? :specification_version then
    s.specification_version = 4
  end

  if s.respond_to? :add_runtime_dependency then
    s.add_runtime_dependency(%q<nokogiri>.freeze, [">= 0"])
    s.add_development_dependency(%q<minitest>.freeze, ["~> 5.0"])
    s.add_development_dependency(%q<minitest-parallel_fork>.freeze, ["~> 1.2"])
    s.add_development_dependency(%q<rake>.freeze, ["~> 13.0"])
    s.add_development_dependency(%q<rake-compiler>.freeze, ["~> 1.1"])
    s.add_development_dependency(%q<rspec-core>.freeze, [">= 0"])
    s.add_development_dependency(%q<rubocop>.freeze, ["~> 1.22"])
    s.add_development_dependency(%q<rubocop-shopify>.freeze, ["~> 2.3"])
  else
    s.add_dependency(%q<nokogiri>.freeze, [">= 0"])
    s.add_dependency(%q<minitest>.freeze, ["~> 5.0"])
    s.add_dependency(%q<minitest-parallel_fork>.freeze, ["~> 1.2"])
    s.add_dependency(%q<rake>.freeze, ["~> 13.0"])
    s.add_dependency(%q<rake-compiler>.freeze, ["~> 1.1"])
    s.add_dependency(%q<rspec-core>.freeze, [">= 0"])
    s.add_dependency(%q<rubocop>.freeze, ["~> 1.22"])
    s.add_dependency(%q<rubocop-shopify>.freeze, ["~> 2.3"])
  end
end
