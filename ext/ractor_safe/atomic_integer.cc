#include "ractor_safe.h"
#include <atomic>

extern "C" {

VALUE rb_cAtomicInteger;

struct AtomicInteger {
    std::atomic<long> value;

    AtomicInteger(long initial_value = 0) : value(initial_value) {}
};

static void ai_free(void *ptr) {
    AtomicInteger *ai = static_cast<AtomicInteger*>(ptr);
    delete ai;
}

static size_t ai_memsize(const void *ptr) {
    return sizeof(AtomicInteger);
}

static const rb_data_type_t atomic_integer_type = {
    "RactorSafe::AtomicInteger",
    {0, ai_free, ai_memsize},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_FROZEN_SHAREABLE
};

static VALUE ai_alloc(VALUE klass) {
    AtomicInteger *ai = new AtomicInteger();
    VALUE obj = TypedData_Wrap_Struct(klass, &atomic_integer_type, ai);
    FL_SET_RAW(obj, RUBY_FL_SHAREABLE);
    return obj;
}

static VALUE ai_initialize(int argc, VALUE *argv, VALUE self) {
    VALUE initial_value;
    rb_scan_args(argc, argv, "01", &initial_value);

    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    long val = NIL_P(initial_value) ? 0 : NUM2LONG(initial_value);
    ai->value.store(val);

    return self;
}

static VALUE ai_get(VALUE self) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    return LONG2NUM(ai->value.load());
}

static VALUE ai_set(VALUE self, VALUE new_value) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    long val = NUM2LONG(new_value);
    ai->value.store(val);

    return new_value;
}

static VALUE ai_increment(VALUE self) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    return LONG2NUM(ai->value.fetch_add(1) + 1);
}

static VALUE ai_decrement(VALUE self) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    return LONG2NUM(ai->value.fetch_sub(1) - 1);
}

static VALUE ai_add(VALUE self, VALUE delta) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    long d = NUM2LONG(delta);
    return LONG2NUM(ai->value.fetch_add(d) + d);
}

static VALUE ai_subtract(VALUE self, VALUE delta) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    long d = NUM2LONG(delta);
    return LONG2NUM(ai->value.fetch_sub(d) - d);
}

static VALUE ai_compare_and_set(VALUE self, VALUE expected, VALUE new_value) {
    AtomicInteger *ai;
    TypedData_Get_Struct(self, AtomicInteger, &atomic_integer_type, ai);

    long exp = NUM2LONG(expected);
    long new_val = NUM2LONG(new_value);

    return ai->value.compare_exchange_strong(exp, new_val) ? Qtrue : Qfalse;
}

void Init_atomic_integer(void) {
    rb_cAtomicInteger = rb_define_class_under(rb_mRactorSafe, "AtomicInteger", rb_cObject);
    rb_define_alloc_func(rb_cAtomicInteger, ai_alloc);
    rb_define_method(rb_cAtomicInteger, "initialize", ai_initialize, -1);
    rb_define_method(rb_cAtomicInteger, "value", ai_get, 0);
    rb_define_method(rb_cAtomicInteger, "value=", ai_set, 1);
    rb_define_method(rb_cAtomicInteger, "increment", ai_increment, 0);
    rb_define_method(rb_cAtomicInteger, "decrement", ai_decrement, 0);
    rb_define_method(rb_cAtomicInteger, "add", ai_add, 1);
    rb_define_method(rb_cAtomicInteger, "subtract", ai_subtract, 1);
    rb_define_method(rb_cAtomicInteger, "compare_and_set", ai_compare_and_set, 2);
}

} // extern "C"