liquid_lib_dir = $LOAD_PATH.detect{ |p| File.exist?(File.join(p, 'liquid.rb')) }
liquid_test_dir = File.join(File.dirname(liquid_lib_dir), 'test')
$LOAD_PATH << liquid_test_dir

require 'test_helper'
require 'liquid/c'

test_files = FileList[File.join(liquid_test_dir, 'integration/**/*_test.rb')]

# Test the Variable#strict_parse patch
test_files << File.join(liquid_test_dir, 'unit/variable_unit_test.rb')

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
