# -*- encoding: utf-8 -*-
# stub: spy 0.4.1 ruby lib

Gem::Specification.new do |s|
  s.name = "spy".freeze
  s.version = "0.4.1"

  s.required_rubygems_version = Gem::Requirement.new(">= 0".freeze) if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib".freeze]
  s.authors = ["Ryan Ong".freeze]
  s.date = "2013-05-28"
  s.description = "A simple modern mocking library that uses the spy pattern and checks method's existence and arity.".freeze
  s.email = ["ryanong@gmail.com".freeze]
  s.homepage = "https://github.com/ryanong/spy".freeze
  s.licenses = ["MIT".freeze]
  s.required_ruby_version = Gem::Requirement.new(">= 1.9.3".freeze)
  s.rubygems_version = "3.3.7".freeze
  s.summary = "Spy is a mocking library that was made for the modern age. It supports only 1.9.3+. Spy by default will raise an error if you attempt to stub a method that doesn't exist or call the stubbed method with the wrong arity.".freeze

  s.installed_by_version = "3.3.7" if s.respond_to? :installed_by_version

  if s.respond_to? :specification_version then
    s.specification_version = 4
  end

  if s.respond_to? :add_runtime_dependency then
    s.add_development_dependency(%q<pry>.freeze, [">= 0"])
    s.add_development_dependency(%q<pry-nav>.freeze, [">= 0"])
    s.add_development_dependency(%q<minitest>.freeze, [">= 4.5.0"])
    s.add_development_dependency(%q<rspec-core>.freeze, [">= 0"])
    s.add_development_dependency(%q<rspec-expectations>.freeze, [">= 0"])
    s.add_development_dependency(%q<coveralls>.freeze, [">= 0"])
  else
    s.add_dependency(%q<pry>.freeze, [">= 0"])
    s.add_dependency(%q<pry-nav>.freeze, [">= 0"])
    s.add_dependency(%q<minitest>.freeze, [">= 4.5.0"])
    s.add_dependency(%q<rspec-core>.freeze, [">= 0"])
    s.add_dependency(%q<rspec-expectations>.freeze, [">= 0"])
    s.add_dependency(%q<coveralls>.freeze, [">= 0"])
  end
end
