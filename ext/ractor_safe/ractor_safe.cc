#include "ractor_safe.h"
#include <tbb/concurrent_hash_map.h>
#include <memory>
#include <string>

extern "C" {
#ifdef HAVE_RB_RACTOR_SHAREABLE_P
#include <ruby/ractor.h>
#endif

VALUE rb_mRactorSafe;
VALUE rb_cHashMap;

struct RubyHasher {
    static size_t hash(VALUE key) {
        return NUM2SIZET(rb_hash(key));
    }
    
    static bool equal(VALUE a, VALUE b) {
        return RTEST(rb_eql(a, b));
    }
};

struct HashMap {
    using MapType = tbb::concurrent_hash_map<VALUE, VALUE, RubyHasher>;
    std::unique_ptr<MapType> map;

    HashMap() : map(std::make_unique<MapType>()) {}
};

static void hm_mark(void *ptr) {
    HashMap *hm = static_cast<HashMap*>(ptr);
    if (hm && hm->map) {
        for (auto it = hm->map->begin(); it != hm->map->end(); ++it) {
            rb_gc_mark(it->first);
            rb_gc_mark(it->second);
        }
    }
}

static void hm_free(void *ptr) {
    HashMap *hm = static_cast<HashMap*>(ptr);
    delete hm;
}

static size_t hm_memsize(const void *ptr) {
    const HashMap *hm = static_cast<const HashMap*>(ptr);
    if (!hm || !hm->map) return 0;
    return sizeof(HashMap) + hm->map->size() * (sizeof(VALUE) * 2 + sizeof(void*) * 4);
}

static const rb_data_type_t hash_map_type = {
    "RactorSafe::HashMap",
    {hm_mark, hm_free, hm_memsize},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE
};

static VALUE hm_alloc(VALUE klass) {
    HashMap *hm = new HashMap();
    return TypedData_Wrap_Struct(klass, &hash_map_type, hm);
}

static VALUE hm_initialize(VALUE self) {
    FL_SET_RAW(self, RUBY_FL_SHAREABLE);
    return self;
}

static VALUE hm_get(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);
    
    HashMap::MapType::const_accessor acc;
    if (hm->map->find(acc, key)) {
        return acc->second;
    }
    return Qnil;
}

static VALUE hm_set(VALUE self, VALUE key, VALUE value) {
    // Ensure both key and value are shareable
    if (!rb_ractor_shareable_p(key)) {
        rb_raise(rb_eArgError, "key must be shareable");
    }
    if (!rb_ractor_shareable_p(value)) {
        rb_raise(rb_eArgError, "value must be shareable");
    }
    
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    HashMap::MapType::accessor acc;
    hm->map->insert(acc, key);
    VALUE old_value = acc->second;
    acc->second = value;
    RB_OBJ_WRITTEN(self, old_value, value);
    
    return value;
}

static VALUE hm_delete(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);
    
    HashMap::MapType::accessor acc;
    if (hm->map->find(acc, key)) {
        VALUE value = acc->second;
        hm->map->erase(acc);
        return value;
    }
    return Qnil;
}

static VALUE hm_size(VALUE self) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);
    
    return SIZET2NUM(hm->map->size());
}

static VALUE hm_clear(VALUE self) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);
    
    hm->map->clear();
    return Qnil;
}

static VALUE hm_has_key(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);
    
    HashMap::MapType::const_accessor acc;
    return hm->map->find(acc, key) ? Qtrue : Qfalse;
}

RUBY_FUNC_EXPORTED void
Init_ractor_safe(void)
{
#ifdef HAVE_RB_EXT_RACTOR_SAFE
    rb_ext_ractor_safe(true);
#endif
    
    rb_mRactorSafe = rb_define_module("RactorSafe");
    
    rb_cHashMap = rb_define_class_under(rb_mRactorSafe, "HashMap", rb_cObject);
    rb_define_alloc_func(rb_cHashMap, hm_alloc);
    rb_define_method(rb_cHashMap, "initialize", hm_initialize, 0);
    rb_define_method(rb_cHashMap, "[]", hm_get, 1);
    rb_define_method(rb_cHashMap, "[]=", hm_set, 2);
    rb_define_method(rb_cHashMap, "delete", hm_delete, 1);
    rb_define_method(rb_cHashMap, "size", hm_size, 0);
    rb_define_method(rb_cHashMap, "length", hm_size, 0);
    rb_define_method(rb_cHashMap, "clear", hm_clear, 0);
    rb_define_method(rb_cHashMap, "has_key?", hm_has_key, 1);
    rb_define_method(rb_cHashMap, "key?", hm_has_key, 1);
    rb_define_method(rb_cHashMap, "include?", hm_has_key, 1);
}

} // extern "C"