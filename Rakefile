require 'rake'
require 'rake/testtask'
require 'bundler/gem_tasks'
require 'rake/extensiontask'

Rake::ExtensionTask.new("liquid_c")

task :default => :test

task :test => ['test:unit', 'test:liquid']

namespace :test do
  Rake::TestTask.new(:unit => :compile) do |t|
    t.libs << 'lib' << 'test'
    t.test_files = FileList['test/unit/**/*_test.rb']
  end

  desc 'run test suite with default parser'
  Rake::TestTask.new(:base_liquid => :compile) do |t|
    t.libs << 'lib'
    t.test_files = ['test/liquid_test.rb']
  end

  desc 'runs test suite with both strict and lax parsers'
  task :liquid do
    ENV['LIQUID_PARSER_MODE'] = 'lax'
    Rake::Task['test:base_liquid'].invoke
    ENV['LIQUID_PARSER_MODE'] = 'strict'
    Rake::Task['test:base_liquid'].reenable
    Rake::Task['test:base_liquid'].invoke
  end
end

namespace :benchmark do
  desc "Run the liquid benchmark with lax parsing"
  task :run do
    ruby "./performance.rb benchmark lax"
  end

  desc "Run the liquid benchmark with strict parsing"
  task :strict do
    ruby "./performance.rb benchmark strict"
  end
end


namespace :profile do
  desc "Run the liquid profile/performance coverage"
  task :run do
    ruby "./performance.rb profile lax"
  end

  desc "Run the liquid profile/performance coverage with strict parsing"
  task :strict do
    ruby "./performance.rb profile strict"
  end
end
