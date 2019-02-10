# Liquid::C
[![Build Status](https://travis-ci.org/Shopify/liquid-c.svg?branch=master)](https://travis-ci.org/Shopify/liquid-c)

Partial native implementation of the liquid ruby gem in C.

## Installation

Add these lines to your application's Gemfile:

    gem 'liquid', github: 'Shopify/liquid', branch: 'master'
    gem 'liquid-c', github: 'Shopify/liquid-c', branch: 'master'

And then execute:

    $ bundle

## Usage

    require 'liquid/c'

then just use the documented API for the liquid Gem.

## Restrictions

* Input strings are assumed to be UTF-8 encoded strings
* Tag#parse(tokens) is given a Liquid::Tokenizer object, instead
  of an array of strings, which only implements the shift method
  to get the next token.

## Performance

To compare Liquid-C's performance with plain Liquid run

    bundle exec rake compare:lax

The latest benchmark results are shown below:

```
$ bundle exec rake compare:lax
/usr/local/Cellar/ruby/2.5.0_2/bin/ruby ./performance.rb bare benchmark lax

Running benchmark for 10 seconds (with 5 seconds warmup).

Warming up --------------------------------------
              parse:     2.000  i/100ms
             render:     5.000  i/100ms
     parse & render:     1.000  i/100ms
Calculating -------------------------------------
              parse:     25.360  (±11.8%) i/s -    250.000  in  10.009777s
             render:     61.636  (±11.4%) i/s -    610.000  in  10.056084s
     parse & render:     17.421  (±11.5%) i/s -    170.000  in  10.018409s

/usr/local/Cellar/ruby/2.5.0_2/bin/ruby ./performance.rb c benchmark lax

Running benchmark for 10 seconds (with 5 seconds warmup).

Warming up --------------------------------------
              parse:    10.000  i/100ms
             render:     6.000  i/100ms
     parse & render:     3.000  i/100ms
Calculating -------------------------------------
              parse:    104.037  (± 3.8%) i/s -      1.040k in  10.013895s
             render:     61.480  (±14.6%) i/s -    600.000  in  10.010273s
     parse & render:     37.443  (±10.7%) i/s -    369.000  in  10.024362s
```

## Developing

    bundle install
    # run tests
    bundle exec rake

## Contributing

1. Fork it ( http://github.com/Shopify/liquid-c/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
