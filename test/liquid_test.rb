liquid_lib_dir = $LOAD_PATH.detect{ |p| File.exists?(File.join(p, 'liquid.rb')) }
liquid_test_dir = File.join(File.dirname(liquid_lib_dir), 'test')
$LOAD_PATH << liquid_test_dir

require 'test_helper'
require 'liquid/c'

test_files = FileList[File.join(liquid_test_dir, 'integration/**/*_test.rb')]
test_files.each do |test_file|
  require test_file
end
