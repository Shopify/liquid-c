# frozen_string_literal: true

namespace :test do
  desc 'run test suite with default parser'
  Rake::TestTask.new(integration: :compile) do |t|
    t.libs << 'lib'
    t.test_files = ['test/integration_test.rb']
  end

  namespace :integration do
    integration_test_with_env = lambda do |env_vars|
      proc do
        old_env_values = ENV.to_hash.slice(*env_vars.keys)
        task = Rake::Task['test:integration']
        begin
          env_vars.each { |key, value| ENV[key] = value }
          task.invoke
        ensure
          old_env_values.each { |key, value| ENV[key] = value }
          task.reenable
        end
      end
    end

    task :lax, &integration_test_with_env.call('LIQUID_PARSER_MODE' => 'lax')

    task :strict, &integration_test_with_env.call('LIQUID_PARSER_MODE' => 'strict')

    task :without_vm, &integration_test_with_env.call('LIQUID_C_DISABLE_VM' => 'true')

    task all: [:lax, :strict, :without_vm]
  end
end
