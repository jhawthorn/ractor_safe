#include "ractor_safe.h"
#include <tbb/concurrent_hash_map.h>
#include <memory>
#include <string>

extern "C" {

VALUE rb_mRactorSafe;
VALUE rb_cConcurrentHashMap;

struct ConcurrentHashMap {
    using MapType = tbb::concurrent_hash_map<VALUE, VALUE>;
    std::unique_ptr<MapType> map;
    
    ConcurrentHashMap() : map(std::make_unique<MapType>()) {}
};

static void chm_mark(void *ptr) {
    ConcurrentHashMap *chm = static_cast<ConcurrentHashMap*>(ptr);
    if (chm && chm->map) {
        for (auto it = chm->map->begin(); it != chm->map->end(); ++it) {
            rb_gc_mark(it->first);
            rb_gc_mark(it->second);
        }
    }
}

static void chm_free(void *ptr) {
    ConcurrentHashMap *chm = static_cast<ConcurrentHashMap*>(ptr);
    delete chm;
}

static size_t chm_memsize(const void *ptr) {
    const ConcurrentHashMap *chm = static_cast<const ConcurrentHashMap*>(ptr);
    if (!chm || !chm->map) return 0;
    return sizeof(ConcurrentHashMap) + chm->map->size() * (sizeof(VALUE) * 2 + sizeof(void*) * 4);
}

static const rb_data_type_t concurrent_hash_map_type = {
    "RactorSafe::ConcurrentHashMap",
    {chm_mark, chm_free, chm_memsize},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE
};

static VALUE chm_alloc(VALUE klass) {
    ConcurrentHashMap *chm = new ConcurrentHashMap();
    return TypedData_Wrap_Struct(klass, &concurrent_hash_map_type, chm);
}

static VALUE chm_initialize(VALUE self) {
    rb_check_frozen(self);
    OBJ_FREEZE(self);
    return self;
}

static VALUE chm_get(VALUE self, VALUE key) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    ConcurrentHashMap::MapType::const_accessor acc;
    if (chm->map->find(acc, key)) {
        return acc->second;
    }
    return Qnil;
}

static VALUE chm_set(VALUE self, VALUE key, VALUE value) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    ConcurrentHashMap::MapType::accessor acc;
    chm->map->insert(acc, key);
    VALUE old_value = acc->second;
    acc->second = value;
    RB_OBJ_WRITTEN(self, old_value, value);
    
    return value;
}

static VALUE chm_delete(VALUE self, VALUE key) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    ConcurrentHashMap::MapType::accessor acc;
    if (chm->map->find(acc, key)) {
        VALUE value = acc->second;
        chm->map->erase(acc);
        return value;
    }
    return Qnil;
}

static VALUE chm_size(VALUE self) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    return SIZET2NUM(chm->map->size());
}

static VALUE chm_clear(VALUE self) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    chm->map->clear();
    return Qnil;
}

static VALUE chm_has_key(VALUE self, VALUE key) {
    ConcurrentHashMap *chm;
    TypedData_Get_Struct(self, ConcurrentHashMap, &concurrent_hash_map_type, chm);
    
    ConcurrentHashMap::MapType::const_accessor acc;
    return chm->map->find(acc, key) ? Qtrue : Qfalse;
}

RUBY_FUNC_EXPORTED void
Init_ractor_safe(void)
{
#ifdef HAVE_RB_EXT_RACTOR_SAFE
    rb_ext_ractor_safe(true);
#endif
    
    rb_mRactorSafe = rb_define_module("RactorSafe");
    
    rb_cConcurrentHashMap = rb_define_class_under(rb_mRactorSafe, "ConcurrentHashMap", rb_cObject);
    rb_define_alloc_func(rb_cConcurrentHashMap, chm_alloc);
    rb_define_method(rb_cConcurrentHashMap, "initialize", chm_initialize, 0);
    rb_define_method(rb_cConcurrentHashMap, "[]", chm_get, 1);
    rb_define_method(rb_cConcurrentHashMap, "[]=", chm_set, 2);
    rb_define_method(rb_cConcurrentHashMap, "delete", chm_delete, 1);
    rb_define_method(rb_cConcurrentHashMap, "size", chm_size, 0);
    rb_define_method(rb_cConcurrentHashMap, "length", chm_size, 0);
    rb_define_method(rb_cConcurrentHashMap, "clear", chm_clear, 0);
    rb_define_method(rb_cConcurrentHashMap, "has_key?", chm_has_key, 1);
    rb_define_method(rb_cConcurrentHashMap, "key?", chm_has_key, 1);
    rb_define_method(rb_cConcurrentHashMap, "include?", chm_has_key, 1);
}

} // extern "C"