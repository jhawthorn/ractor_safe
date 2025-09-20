# frozen_string_literal: true

require "mkmf"

# Makes all symbols private by default to avoid unintended conflict
# with other gems. To explicitly export symbols you can use RUBY_FUNC_EXPORTED
# selectively, or entirely remove this flag.
append_cflags("-fvisibility=hidden")

# Check for C++ compiler
have_library("stdc++")

# Check for Ractor safety functions
have_func("rb_ext_ractor_safe", "ruby.h")
have_func("rb_ractor_shareable_p", "ruby/ractor.h")


create_makefile("ractor_safe/ractor_safe")
