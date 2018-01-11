require 'mkmf'
$CFLAGS << ' -Wall -Werror -Wextra -Wno-unused-parameter -Wno-missing-field-initializers'
compiler = RbConfig::MAKEFILE_CONFIG['CC']
if ENV['DEBUG'] == 'true' && compiler =~ /gcc|g\+\+/
  $CFLAGS << ' -fbounds-check'
end
$warnflags.gsub!(/-Wdeclaration-after-statement/, "") if $warnflags
create_makefile("liquid_c")
