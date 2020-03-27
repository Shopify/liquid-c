require 'minitest/autorun'
require 'liquid/c'

if GC.respond_to?(:compact)
  GC.compact
end
