require 'mkmf'
$CFLAGS << ' -Wall -Werror -fbounds-check'
$warnflags.gsub!(/-Wdeclaration-after-statement/, "")
create_makefile("liquid_c")
