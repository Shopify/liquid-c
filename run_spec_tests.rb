# frozen_string_literal: true

# Standalone spec test runner for liquid-c
# This runs liquid-spec YAML tests directly without the liquid-spec gem's runner
# to avoid version compatibility issues with Liquid::Environment

require "bundler/setup"
require "liquid/c"
require "yaml"
require "optparse"

# Color codes for terminal output
class Colors
  RESET = "\e[0m"
  GREEN = "\e[32m"
  RED = "\e[31m"
  YELLOW = "\e[33m"
  CYAN = "\e[36m"
  GRAY = "\e[90m"
end

class SpecRunner
  attr_reader :passed, :failed, :errors, :failures

  def initialize(spec_dir:, pattern: nil, verbose: false, max_failures: 10)
    @spec_dir = spec_dir
    @pattern = pattern
    @verbose = verbose
    @max_failures = max_failures
    @passed = 0
    @failed = 0
    @errors = 0
    @failures = []
  end

  def run
    spec_files = Dir.glob(File.join(@spec_dir, "**/*.yml")).sort

    spec_files.each do |file|
      break if @max_failures && @failures.size >= @max_failures

      run_spec_file(file)
    end

    print_summary
    @failed == 0 && @errors == 0
  end

  private

  def run_spec_file(file)
    data = YAML.safe_load(File.read(file), permitted_classes: [Symbol])
    return unless data.is_a?(Hash) && data["specs"]

    specs = data["specs"]
    specs.each do |spec|
      break if @max_failures && @failures.size >= @max_failures

      next if @pattern && !spec["name"].to_s.match?(@pattern)

      run_spec(spec, file)
    end
  end

  def run_spec(spec, file)
    name = spec["name"]
    template_source = spec["template"]
    expected = spec["expected"]
    environment = spec["environment"] || {}
    error_mode = spec["error_mode"]

    begin
      parse_options = {}
      parse_options[:error_mode] = error_mode.to_sym if error_mode

      template = Liquid::Template.parse(template_source, **parse_options)
      result = template.render(environment)

      if result == expected
        @passed += 1
        print "#{Colors::GREEN}.#{Colors::RESET}" unless @verbose
        puts "#{Colors::GREEN}PASS#{Colors::RESET} #{name}" if @verbose
      else
        @failed += 1
        @failures << {
          name: name,
          file: file,
          template: template_source,
          expected: expected,
          actual: result,
          environment: environment,
        }
        print "#{Colors::RED}F#{Colors::RESET}" unless @verbose
        if @verbose
          puts "#{Colors::RED}FAIL#{Colors::RESET} #{name}"
          puts "  Template: #{template_source.inspect}"
          puts "  Expected: #{expected.inspect}"
          puts "  Actual:   #{result.inspect}"
        end
      end
    rescue StandardError => e
      @errors += 1
      @failures << {
        name: name,
        file: file,
        template: template_source,
        expected: expected,
        error: "#{e.class}: #{e.message}",
        environment: environment,
      }
      print "#{Colors::RED}E#{Colors::RESET}" unless @verbose
      if @verbose
        puts "#{Colors::RED}ERROR#{Colors::RESET} #{name}"
        puts "  #{e.class}: #{e.message}"
      end
    end
  end

  def print_summary
    puts
    puts

    if @failures.any?
      puts "#{Colors::RED}Failures:#{Colors::RESET}"
      puts
      @failures.each_with_index do |failure, i|
        puts "#{i + 1}) #{failure[:name]}"
        puts "   File: #{failure[:file]}"
        puts "   Template: #{failure[:template].inspect}"
        puts "   Environment: #{failure[:environment].inspect}" if failure[:environment].any?
        if failure[:error]
          puts "   #{Colors::RED}Error: #{failure[:error]}#{Colors::RESET}"
        else
          puts "   Expected: #{failure[:expected].inspect}"
          puts "   Actual:   #{failure[:actual].inspect}"
        end
        puts
      end
    end

    total = @passed + @failed + @errors
    puts "#{total} specs, #{Colors::GREEN}#{@passed} passed#{Colors::RESET}, " \
         "#{@failed > 0 ? Colors::RED : Colors::GRAY}#{@failed} failed#{Colors::RESET}, " \
         "#{@errors > 0 ? Colors::RED : Colors::GRAY}#{@errors} errors#{Colors::RESET}"
  end
end

# Parse command line options
options = {
  verbose: false,
  max_failures: 10,
}

OptionParser.new do |opts|
  opts.banner = "Usage: #{$0} [options] [SPEC_DIR]"

  opts.on("-n", "--name PATTERN", "Filter specs by name pattern") do |p|
    options[:pattern] = Regexp.new(p, Regexp::IGNORECASE)
  end

  opts.on("-v", "--verbose", "Show verbose output") do
    options[:verbose] = true
  end

  opts.on("--max-failures N", Integer, "Stop after N failures (default: 10)") do |n|
    options[:max_failures] = n
  end

  opts.on("--no-max-failures", "Run all specs regardless of failures") do
    options[:max_failures] = nil
  end

  opts.on("-h", "--help", "Show this help") do
    puts opts
    exit
  end
end.parse!

# Default spec directory to liquid-spec gem's specs
spec_dir = ARGV[0]
unless spec_dir
  liquid_spec_gem = Gem::Specification.find_by_name("liquid-spec") rescue nil
  if liquid_spec_gem
    spec_dir = File.join(liquid_spec_gem.gem_dir, "specs", "basics")
  else
    abort "No spec directory provided and liquid-spec gem not found"
  end
end

unless File.directory?(spec_dir)
  abort "Spec directory not found: #{spec_dir}"
end

puts "Running specs from: #{spec_dir}"
puts

runner = SpecRunner.new(
  spec_dir: spec_dir,
  pattern: options[:pattern],
  verbose: options[:verbose],
  max_failures: options[:max_failures]
)

exit(runner.run ? 0 : 1)
