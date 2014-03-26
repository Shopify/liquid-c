require 'rake'
require 'rake/testtask'
require 'bundler/gem_tasks'
require 'rake/extensiontask'

Rake::ExtensionTask.new("liquid_c")

task :default => :test

liquid_lib_dir = $LOAD_PATH.detect{ |p| File.exists?(File.join(p, 'liquid.rb')) }

desc 'run test suite with default parser'
Rake::TestTask.new(:base_test => :compile) do |t|
  t.libs << 'lib' << 'test' << liquid_lib_dir
  t.ruby_opts = ["-rliquid/c"]
  t.test_files = FileList['test/**/*_test.rb']
  t.verbose = false
end

desc 'run test suite with warn error mode'
task :warn_test do
  ENV['LIQUID_PARSER_MODE'] = 'warn'
  Rake::Task['base_test'].invoke
end

desc 'runs test suite with both strict and lax parsers'
task :test do
  ENV['LIQUID_PARSER_MODE'] = 'lax'
  Rake::Task['base_test'].invoke
  ENV['LIQUID_PARSER_MODE'] = 'strict'
  Rake::Task['base_test'].reenable
  Rake::Task['base_test'].invoke
end
