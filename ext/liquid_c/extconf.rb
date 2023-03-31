# frozen_string_literal: true

require "mkmf"
$CFLAGS << " -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
append_cflags("-fvisibility=hidden")
# In Ruby 2.6 and earlier, the Ruby headers did not have struct timespec defined
valid_headers = RbConfig::CONFIG["host_os"] !~ /linux/ || Gem::Version.new(RUBY_VERSION) >= Gem::Version.new("2.7")
pedantic = !ENV["LIQUID_C_PEDANTIC"].to_s.empty?
if pedantic && valid_headers
  $CFLAGS << " -Werror"
end
if ENV["DEBUG"] == "true"
  append_cflags("-fbounds-check")
  CONFIG["optflags"] = " -O0"
  # Hack to enable assertions since ruby/assert.h disables assertions unless
  # Ruby was compiled with -DRUBY_DEBUG.
  # https://github.com/ruby/ruby/blob/9e678cdbd054f78576a8f21b3f97cccc395ade22/include/ruby/assert.h#L36-L41
  $CFLAGS << " -DRUBY_DEBUG"
else
  $CFLAGS << " -DNDEBUG"
end

if Gem::Version.new(RUBY_VERSION) >= Gem::Version.new("2.7.0") # added in 2.7
  $CFLAGS << " -DHAVE_RB_HASH_BULK_INSERT"
end

$warnflags&.gsub!(/-Wdeclaration-after-statement/, "")
create_makefile("liquid_c")
