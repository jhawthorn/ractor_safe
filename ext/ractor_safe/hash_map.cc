#include "ractor_safe.h"
#include <unordered_map>
#include <memory>
#include <mutex>

extern "C" {
#ifdef HAVE_RB_RACTOR_SHAREABLE_P
#include <ruby/ractor.h>
#endif

VALUE rb_cHashMap;

struct RubyHasher {
    size_t operator()(VALUE key) const {
        return NUM2SIZET(rb_hash(key));
    }
};

struct RubyEqual {
    bool operator()(VALUE a, VALUE b) const {
        return RTEST(rb_eql(a, b));
    }
};

struct HashMap {
    using MapType = std::unordered_map<VALUE, VALUE, RubyHasher, RubyEqual>;
    std::unique_ptr<MapType> map;
    std::mutex mutex;

    HashMap() : map(std::make_unique<MapType>()) {}
};

static void hm_mark(void *ptr) {
    HashMap *hm = static_cast<HashMap*>(ptr);
    if (hm && hm->map) {
        std::lock_guard<std::mutex> lock(hm->mutex);
        for (HashMap::MapType::const_iterator it = hm->map->begin(); it != hm->map->end(); ++it) {
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
    VALUE obj = TypedData_Wrap_Struct(klass, &hash_map_type, hm);
    FL_SET_RAW(obj, RUBY_FL_SHAREABLE);
    return obj;
}

static VALUE hm_get(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    std::lock_guard<std::mutex> lock(hm->mutex);
    auto it = hm->map->find(key);
    if (it != hm->map->end()) {
        return it->second;
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

    std::lock_guard<std::mutex> lock(hm->mutex);
    (*hm->map)[key] = value;
    RB_OBJ_WRITTEN(self, Qundef, value);

    return value;
}

static VALUE hm_delete(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    std::lock_guard<std::mutex> lock(hm->mutex);
    auto it = hm->map->find(key);
    if (it != hm->map->end()) {
        VALUE value = it->second;
        hm->map->erase(it);
        return value;
    }
    return Qnil;
}

static VALUE hm_size(VALUE self) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    std::lock_guard<std::mutex> lock(hm->mutex);
    return SIZET2NUM(hm->map->size());
}

static VALUE hm_clear(VALUE self) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    std::lock_guard<std::mutex> lock(hm->mutex);
    hm->map->clear();
    return Qnil;
}

static VALUE hm_has_key(VALUE self, VALUE key) {
    HashMap *hm;
    TypedData_Get_Struct(self, HashMap, &hash_map_type, hm);

    std::lock_guard<std::mutex> lock(hm->mutex);
    return hm->map->find(key) != hm->map->end() ? Qtrue : Qfalse;
}

void Init_hash_map(void) {
    rb_cHashMap = rb_define_class_under(rb_mRactorSafe, "HashMap", rb_cObject);
    rb_define_alloc_func(rb_cHashMap, hm_alloc);
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