# frozen_string_literal: true

require "mkmf"

# Makes all symbols private by default to avoid unintended conflict
# with other gems. To explicitly export symbols you can use RUBY_FUNC_EXPORTED
# selectively, or entirely remove this flag.
append_cflags("-fvisibility=hidden")

# Check for C++ compiler
have_library("stdc++")

# Check for OneTBB
unless pkg_config("tbb")
  abort "OneTBB not found. Please install OneTBB development libraries."
end

# Check for Ractor safety function
have_func("rb_ext_ractor_safe", "ruby.h")

# Enable C++17 for OneTBB
$CXXFLAGS = "#{$CXXFLAGS} -std=c++17"

create_makefile("ractor_safe/ractor_safe")
