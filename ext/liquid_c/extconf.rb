require 'mkmf'
$CFLAGS << ' -Wall -Werror'
if ENV['DEBUG'] == 'true'
  $CFLAGS << ' -fbounds-check'
end
$warnflags.gsub!(/-Wdeclaration-after-statement/, "")
create_makefile("liquid_c")
