require 'mkmf'
$CFLAGS << ' -Wall -Werror'
compiler = RbConfig::MAKEFILE_CONFIG['CC']
if ENV['DEBUG'] == 'true' && compiler =~ /gcc|g\+\+/
  $CFLAGS << ' -fbounds-check'
end
$warnflags.gsub!(/-Wdeclaration-after-statement/, "")
create_makefile("liquid_c")
