# frozen_string_literal: true

Liquid::Variable.class_eval do
  def compile_evaluate(code)
    code.add_evaluate_expression(@name)
    filters.each do |filter_name, filter_args, keyword_args|
      filter_args.each do |arg|
        code.add_evaluate_expression(arg)
      end
      num_args = filter_args.size
      if keyword_args
        keyword_args.each do |key, value|
          code.add_evaluate_expression(key)
          code.add_evaluate_expression(value)
        end
        num_args += 1
        code.add_hash_new(keyword_args.size)
      end
      code.add_filter(filter_name, num_args)
    end
  end
end

Liquid::VariableLookup.class_eval do
  def compile_evaluate(code)
    code.add_find_variable(name)
    lookups.each_with_index do |lookup, i|
      is_command = @command_flags & (1 << i) != 0
      if is_command
        code.add_lookup_command(lookup)
      else
        code.add_lookup_key(lookup)
      end
    end
  end
end

Liquid::RangeLookup.class_eval do
  def compile_evaluate(code)
    code.add_evaluate_expression(@start_obj)
    code.add_evaluate_expression(@end_obj)
    code.add_new_int_range
  end
end

Liquid::Tag.class_eval do
  # Avoid automatically inheriting compile methods other than the base compile
  # method, so that compile methods can be added without breaking subclasses
  # that override the render behaviour.
  def self.inherited(subclass)
    alias_method(:compile, :base_compile)
    super
  end

  alias_method(:base_compile, :compile)
end

Liquid::Comment.class_eval do
  def compile(_code)
  end
end

Liquid::Raw.class_eval do
  def compile(code)
    code.add_write_raw(@body)
  end
end
