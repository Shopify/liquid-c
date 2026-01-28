# frozen_string_literal: true

task :rubocop do
  require "rubocop/rake_task"
  ENV["RUBOCOP_CACHE_ROOT"] ||= File.expand_path("../tmp/rubocop_cache", __dir__)
  RuboCop::RakeTask.new
end
