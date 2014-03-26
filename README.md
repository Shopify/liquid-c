# Liquid::C
[![Build Status](https://api.travis-ci.org/Shopify/liquid-c.png?branch=master)](https://travis-ci.org/Shopify/liquid-c)

Partial native implementation of the liquid ruby gem in C.

## Installation

Add this line to your application's Gemfile:

    gem 'liquid-c'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install liquid-c

## Usage

    require 'liquid/c'

then just use the documented API for the liquid Gem.

## Restrictions

* Tag#parse(tokens) is given a Liquid::Tokenizer object, instead
  of an array of strings, which only implements the shift method
  to get the next token.

## Contributing

1. Fork it ( http://github.com/Shopify/liquid-c/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
