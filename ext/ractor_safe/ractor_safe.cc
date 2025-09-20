#include "ractor_safe.h"

extern "C" {

VALUE rb_mRactorSafe;

// Forward declarations
void Init_hash_map(void);
void Init_queue(void);
void Init_atomic_integer(void);

RUBY_FUNC_EXPORTED void
Init_ractor_safe(void)
{
#ifdef HAVE_RB_EXT_RACTOR_SAFE
    rb_ext_ractor_safe(true);
#endif
    
    rb_mRactorSafe = rb_define_module("RactorSafe");

    Init_hash_map();
    Init_queue();
    Init_atomic_integer();
}

} // extern "C"