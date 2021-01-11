# frozen_string_literal: true
liquid_lib_dir = $LOAD_PATH.detect { |p| File.exist?(File.join(p, 'liquid.rb')) }
liquid_test_dir = File.join(File.dirname(liquid_lib_dir), 'test')
$LOAD_PATH << liquid_test_dir

require 'test_helper'
require 'liquid/c'

if ENV['LIQUID_C_DISABLE_VM']
  puts "-- Liquid-C VM Disabled"
  Liquid::ParseContext.liquid_c_nodes_disabled = true
end

if ENV['LIQUID_C_TEST_SERIALIZE']
  puts "-- Liquid-C serialization"

  module SkipSerializeFailingTestsSetup
    SERIALIZE_SKIPPED_TESTS = %w(
      ErrorHandlingTest#test_warning_line_numbers
      ErrorHandlingTest#test_warnings
      ForTagTest#test_instrument_for_offset_continue
    )

    def setup
      skip if SERIALIZE_SKIPPED_TESTS.include?("#{class_name}##{name}")
      super
    end
  end

  Minitest::Test.prepend(SkipSerializeFailingTestsSetup)

  Liquid::Template.singleton_class.class_eval do
    alias_method :original_parse, :parse

    def parse(source, options = {})
      template = original_parse(source, options)
      return template if template.root.parse_context.liquid_c_nodes_disabled?
      Liquid::Template.load(template.dump, options)
    end
  end
end

test_files = FileList[File.join(liquid_test_dir, 'integration/**/*_test.rb')]
test_files << File.join(liquid_test_dir, 'unit/tokenizer_unit_test.rb')
test_files.each do |test_file|
  require test_file
end

module GCCompactSetup
  def setup
    GC.compact if GC.respond_to?(:compact)
    super
  end
end

Minitest::Test.prepend(GCCompactSetup)
