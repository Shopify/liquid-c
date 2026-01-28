# frozen_string_literal: true

require "rake"
require "rake/testtask"
require "bundler/gem_tasks"
require "rake/extensiontask"
require "benchmark"
require "ruby_memcheck"

ENV["DEBUG"] ||= "true"

default_tasks = [:test]
default_tasks << :rubocop if ENV["LIQUID_C_RUN_RUBOCOP"] == "1"
task default: default_tasks

task :test do
  Rake::Task["test:unit"].invoke
  if ENV["LIQUID_C_RUN_INTEGRATION"] == "1"
    Rake::Task["test:integration:all"].invoke
  end
end

namespace :test do
  task valgrind: ["test:unit:valgrind", "test:integration:valgrind:all"]
end

desc "Run liquid-spec via adapter after unit tests"
task spec: :test do
  sh "bundle exec liquid-spec run liquid_c_adapter.rb -s basics"
end
