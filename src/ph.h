#pragma once

////////////////////////////////////////
//  ==== Settings ====
////////////////////////////////////////

// Define the following overrides before including ph.h.
//
// Slices are GC'd unless:
#define PH_SLICES_ARE_MANUAL

#include "system_includes.h"

////////////////////////////////////////
// Allocator macros
////////////////////////////////////////

#ifdef PH_DEBUG
#define ph_expect(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed expectation at %s, %d\n", __FILE__, __LINE__);\
        fprintf(stderr, "    ---- " #expr "\n");\
    }
#else
#define ph_expect(expr)
#endif

#ifdef PH_DEBUG
#define ph_assert(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed assertion at %s, %d\n", __FILE__, __LINE__);\
        fprintf(stderr, "    ---- " #expr "\n");\
        ph::quit(-1);\
    }
#else
#define ph_assert(expr)
#endif

#define phanaged(type, num) \
        (type*)ph::memory::typeless_managed(sizeof(type) * num)

#define phanaged_realloc(type, old, num) \
    (type*)ph::memory::typeless_managed_realloc(old, sizeof(type) * num)

#define phalloc(type, num) \
    (type*)ph::memory::typeless_alloc(size_t(num) * sizeof(type))

#define phree(mem)\
        ph::memory::typeless_free((void*)(mem)); (mem) = NULL

namespace ph
{

void init();

//////////////////////////
// Int Types
//////////////////////////

typedef int64_t  int64;
typedef uint64_t uint64;
typedef int32_t  int32;

#define PH_MAX_int32 2147483648
#define PH_MAX_int64 9223372036854775808

//////////////////////////
// Memory
//////////////////////////
namespace memory
{
// In debug mode, query dynamic memory allocated.
int64 bytes_allocated();

void* typeless_managed(size_t n_bytes);
//////////// Note: Boehm has had bugs with this for years.
/* void* typeless_managed_realloc(void* old, size_t n_bytes); */

// malloc equivalent. Use phalloc macro
void* typeless_alloc(size_t n_bytes);

// free equivalent. Use phree macro
void typeless_free(void* mem);

}  // ns memory

/////////////////////////
// Logging
/////////////////////////

void log(const char* s);
void logf(const char* fmt, ...);

/////////////////////////

///////////////////////////////
// Hash functions
///////////////////////////////
uint64_t hash(int64 data);

uint64_t hash(const char* s);

/////////////////////////
// Error handling
/////////////////////////
void phatal_error(const char* message);

void quit(int code);

///////////////////////////////////////////////////////////////////////////////
// Slices
///////////////////////////////////////////////////////////////////////////////

// === Slice
template<typename T>
struct Slice
{
    T* ptr;
    size_t n_elems;
    size_t n_capacity;
    T& operator[](const int64 i)
    {
#ifdef PH_DEBUG
        if ((size_t)i >= n_elems || i < 0)
        {
            log("slice access fail");  // Serving as a place to place breakpoints.
        }
#endif
        ph_assert(i >= 0);
        ph_assert((size_t )i < n_elems);
        return ptr[i];
    }
};

// Create a Slice
template<typename T>
Slice<T> MakeSlice(size_t n_capacity)
{
    Slice<T> slice;
    slice.n_capacity = n_capacity;
    slice.n_elems = 0;
#if defined(PH_SLICES_ARE_MANUAL)
    slice.ptr = phalloc(T, n_capacity);
#else
    slice.ptr = phanaged(T, n_capacity);
#endif
    return slice;
}

template<typename T>
#if defined(PH_SLICES_ARE_MANUAL)
void release(Slice<T>* s)
{
    if (s->ptr)
    {
        phree(s->ptr);
    }
#else
void release(Slice<T>*)
{
#ifdef PH_DEBUG
    printf("WARNING: Releasing a memory managed array\n");
#endif
#endif
}

    // Add an element to the end.
    // Returns the location of the element in the array
template<typename T>
int64 append(Slice<T>* slice, T elem)
{
    if (slice->n_elems > 0 && slice->n_capacity == slice->n_elems)
    {
        slice->n_capacity *= 2;
#if defined(PH_SLICES_ARE_MANUAL)
        T* new_mem = phalloc(T, slice->n_capacity);
        memcpy(new_mem, slice->ptr, sizeof(T) * slice->n_elems);
        phree(slice->ptr);
        slice->ptr = new_mem;
#else
        T* new_mem = phanaged(T, slice->n_capacity);
        memcpy(new_mem, slice->ptr, sizeof(T) * slice->n_elems);
        slice->ptr = new_mem;
        /* slice->ptr = phanaged_realloc(T, slice->ptr, */
        /*         sizeof(T) * slice->n_elems); */
#endif
    }
    slice->ptr[slice->n_elems] = elem;
    return (int64)slice->n_elems++;
}
/*
 * NOT IN USE
 * TODO: Remove?
 template<typename T>
 void append_array(Slice<T>* slice, T* elems, int64 n_elems) {
 for(int64 i = 0; i < n_elems; ++i) {
 append(slice, elems[i]);
 }
 }
 */

template<typename T>
Slice<T> slice(Slice<T> orig, int64 begin, int64 end)
{
    ph_assert(begin < end);
    ph_assert(end <= (int64)orig.n_elems);

    Slice<T> out = MakeSlice<T>(orig.n_elems / 2);

    for (auto i = begin; i < end; ++i)
    {
        append(&out, orig[i]);
    }

    return out;
}

template<typename T>
T pop(Slice<T>* slice)
{
    return slice->ptr[--slice->n_elems];
}

template<typename T>
int64 count(Slice<T> slice)
{
    return (int64)slice.n_elems;
}

template<typename T>
int64 find(Slice<T> slice, T to_find, T* out = NULL)
{
    for (int64 i = 0; i < count(slice); ++i)
    {
        T* e = &slice.ptr[i];
        if (*e == to_find){
            if (out)
            {
                *out = *e;
            }
            return i;
        }
    }
    return -1;
}

template<typename T>
void clear(Slice<T>* slice)
{
    slice->n_elems = 0;
}

///////////////////////////////////////////////////////////////////////////////
// Hash map
///////////////////////////////////////////////////////////////////////////////

template<typename K, typename V>
struct Record
{
    K* key;
    V value;
};

template<typename K, typename V>
struct Dict
{
    int64 num_buckets;
    Slice<Record<K, V>>* buckets;
};

template<typename K, typename V>
Dict<K, V> MakeDict(int64 num_buckets)
{
    Dict<K, V> dict;
    typedef Slice<Record<K, V>> record_slice_t;
    dict.buckets = phalloc(record_slice_t, (size_t) num_buckets);
    for (int i = 0; i < num_buckets; ++i)
    {
        dict.buckets[i] = MakeSlice<Record<K,V>>(4);
    }
    dict.num_buckets = num_buckets;
    return dict;
}

///////////////////////////////
// Dict operations
///////////////////////////////

template<typename K, typename V>
void insert(Dict<K, V>* dict, K key, V value)
{
    uint64_t n = (hash(key)) % dict->num_buckets;
    Record<K, V> record;
    record.key = phalloc(K, 1);
    memcpy(record.key, &key, sizeof(key));
    record.value = value;
    append(&dict->buckets[n], record);
}

template<typename K, typename V>
V* find(Dict<K, V>* dict, K key)
{
    uint64_t n = hash(key) % dict->num_buckets;
    Slice<Record<K, V>> bucket = dict->buckets[n];
    for (int i = 0; i < count(bucket); ++i)
    {
        Record<K, V>* record = &bucket[i];
        if (*record->key == key)
        {
            return &record->value;
        }
    }
    return NULL;
}

template<typename K, typename V>
void release(Dict<K, V>* dict)
{
    for (int64 i = 0; i < dict->num_buckets; ++i)
    {
        Slice<Record<K, V>> bucket = dict->buckets[i];
        for (int64 j = 0; j < count(bucket); ++j)
        {
            phree(bucket[j].key);
        }
        release(&dict->buckets[i]);
    }
    phree(dict->buckets);
}

}  // ns ph
