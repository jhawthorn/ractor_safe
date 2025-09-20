#include "ractor_safe.h"
#include <deque>
#include <memory>

extern "C" {
#ifdef HAVE_RB_RACTOR_SHAREABLE_P
#include <ruby/ractor.h>
#endif
#include <ruby/thread_native.h>
#include <ruby/thread.h>

VALUE rb_cQueue;
VALUE rb_eClosedQueueError;

struct Queue {
    std::deque<VALUE> queue;
    rb_nativethread_lock_t lock;
    rb_nativethread_cond_t cond;
    bool closed;

    Queue() : closed(false) {
        rb_native_mutex_initialize(&lock);
        rb_native_cond_initialize(&cond);
    }
    
    ~Queue() {
        rb_native_mutex_destroy(&lock);
        rb_native_cond_destroy(&cond);
    }
};

static void q_mark(void *ptr) {
    Queue *q = static_cast<Queue*>(ptr);
    if (q) {
        for (VALUE value : q->queue) {
            rb_gc_mark(value);
        }
    }
}

static void q_free(void *ptr) {
    Queue *q = static_cast<Queue*>(ptr);
    delete q;
}

static size_t q_memsize(const void *ptr) {
    const Queue *q = static_cast<const Queue*>(ptr);
    if (!q) return 0;
    return sizeof(Queue) + q->queue.size() * sizeof(VALUE);
}

static const rb_data_type_t queue_type = {
    "RactorSafe::Queue",
    {q_mark, q_free, q_memsize},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE
};

static VALUE q_alloc(VALUE klass) {
    Queue *q = new Queue();
    VALUE obj = TypedData_Wrap_Struct(klass, &queue_type, q);
    FL_SET_RAW(obj, RUBY_FL_SHAREABLE);
    return obj;
}


static VALUE q_push(VALUE self, VALUE value) {
    // Ensure value is shareable
    if (!rb_ractor_shareable_p(value)) {
        rb_raise(rb_eArgError, "value must be shareable");
    }

    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);

    rb_native_mutex_lock(&q->lock);
    if (q->closed) {
        rb_native_mutex_unlock(&q->lock);
        rb_raise(rb_eClosedQueueError, "queue is closed");
    }
    q->queue.push_back(value);
    RB_OBJ_WRITTEN(self, Qundef, value);
    rb_native_cond_signal(&q->cond);
    rb_native_mutex_unlock(&q->lock);

    return self;
}

struct pop_wait_data {
    Queue *q;
    VALUE value;
    bool interrupted;
};

static void* q_pop_wait(void *ptr) {
    pop_wait_data *data = static_cast<pop_wait_data*>(ptr);

    rb_native_mutex_lock(&data->q->lock);
    while (data->q->queue.empty() && !data->q->closed && !data->interrupted) {
        rb_native_cond_wait(&data->q->cond, &data->q->lock);
    }
    if (!data->interrupted && !data->q->queue.empty()) {
        data->value = data->q->queue.front();
        data->q->queue.pop_front();
    } else {
        data->value = Qnil;
    }
    rb_native_mutex_unlock(&data->q->lock);

    return NULL;
}

static void q_pop_ubf(void *ptr) {
    pop_wait_data *data = static_cast<pop_wait_data*>(ptr);

    rb_native_mutex_lock(&data->q->lock);
    data->interrupted = true;
    rb_native_cond_broadcast(&data->q->cond);
    rb_native_mutex_unlock(&data->q->lock);
}

static VALUE q_pop(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);

    // Fast path: check if we can pop immediately
    rb_native_mutex_lock(&q->lock);
    if (!q->queue.empty()) {
        VALUE value = q->queue.front();
        q->queue.pop_front();
        rb_native_mutex_unlock(&q->lock);
        return value;
    }
    if (q->closed) {
        rb_native_mutex_unlock(&q->lock);
        return Qnil;
    }
    rb_native_mutex_unlock(&q->lock);

    // Slow path: need to wait, release GVL
    pop_wait_data data = {q, Qnil, false};
    rb_thread_call_without_gvl(q_pop_wait, &data, q_pop_ubf, &data);

    return data.value;
}

static VALUE q_try_pop(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);
    
    rb_native_mutex_lock(&q->lock);
    if (q->queue.empty()) {
        rb_native_mutex_unlock(&q->lock);
        return Qnil;
    }
    VALUE value = q->queue.front();
    q->queue.pop_front();
    rb_native_mutex_unlock(&q->lock);
    
    return value;
}

static VALUE q_empty_p(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);
    
    rb_native_mutex_lock(&q->lock);
    bool empty = q->queue.empty();
    rb_native_mutex_unlock(&q->lock);
    
    return empty ? Qtrue : Qfalse;
}

static VALUE q_size(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);
    
    rb_native_mutex_lock(&q->lock);
    size_t size = q->queue.size();
    rb_native_mutex_unlock(&q->lock);
    
    return SIZET2NUM(size);
}

static VALUE q_clear(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);

    rb_native_mutex_lock(&q->lock);
    q->queue.clear();
    rb_native_mutex_unlock(&q->lock);

    return self;
}

static VALUE q_close(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);

    rb_native_mutex_lock(&q->lock);
    q->closed = true;
    rb_native_cond_broadcast(&q->cond);
    rb_native_mutex_unlock(&q->lock);

    return self;
}

static VALUE q_closed_p(VALUE self) {
    Queue *q;
    TypedData_Get_Struct(self, Queue, &queue_type, q);

    rb_native_mutex_lock(&q->lock);
    bool closed = q->closed;
    rb_native_mutex_unlock(&q->lock);

    return closed ? Qtrue : Qfalse;
}

void Init_queue(void) {
    rb_eClosedQueueError = rb_define_class_under(rb_mRactorSafe, "ClosedQueueError", rb_eStandardError);

    rb_cQueue = rb_define_class_under(rb_mRactorSafe, "Queue", rb_cObject);
    rb_define_alloc_func(rb_cQueue, q_alloc);
    rb_define_method(rb_cQueue, "push", q_push, 1);
    rb_define_method(rb_cQueue, "<<", q_push, 1);
    rb_define_method(rb_cQueue, "enq", q_push, 1);
    rb_define_method(rb_cQueue, "pop", q_pop, 0);
    rb_define_method(rb_cQueue, "deq", q_pop, 0);
    rb_define_method(rb_cQueue, "shift", q_pop, 0);
    rb_define_method(rb_cQueue, "try_pop", q_try_pop, 0);
    rb_define_method(rb_cQueue, "empty?", q_empty_p, 0);
    rb_define_method(rb_cQueue, "size", q_size, 0);
    rb_define_method(rb_cQueue, "length", q_size, 0);
    rb_define_method(rb_cQueue, "clear", q_clear, 0);
    rb_define_method(rb_cQueue, "close", q_close, 0);
    rb_define_method(rb_cQueue, "closed?", q_closed_p, 0);
}

} // extern "C"
