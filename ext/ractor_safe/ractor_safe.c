#include "ractor_safe.h"

VALUE rb_mRactorSafe;

RUBY_FUNC_EXPORTED void
Init_ractor_safe(void)
{
  rb_mRactorSafe = rb_define_module("RactorSafe");
}
