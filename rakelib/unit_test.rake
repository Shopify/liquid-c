# frozen_string_literal: true

namespace :test do
  Rake::TestTask.new(unit: :compile) do |t|
    t.libs << 'lib' << 'test'
    t.test_files = FileList['test/unit/**/*_test.rb']
  end
end
