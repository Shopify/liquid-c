# frozen_string_literal: true
require 'rake'
require 'rake/testtask'
require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'benchmark'

ENV['DEBUG'] ||= 'true'

task default: [:test, :rubocop]

task test: ['test:unit', 'test:integration:all']
