# frozen_string_literal: true
liquid_lib_dir = $LOAD_PATH.detect { |p| File.exist?(File.join(p, 'liquid.rb')) }
liquid_test_dir = File.join(File.dirname(liquid_lib_dir), 'test')
$LOAD_PATH << liquid_test_dir

require 'test_helper'
require 'liquid/c'

if ENV['LIQUID_C_DISABLE_VM']
  puts "-- Liquid-C VM Disabled"
  Liquid::ParseContext.disable_liquid_c_nodes = true
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
