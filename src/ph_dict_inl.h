#pragma once

#include "ph_base.h"


namespace ph {

template<typename K, typename V>
struct Record {
    K* key;
    V value;
};

template<typename K, typename V>
struct Dict {
    int64 num_buckets;
    Slice<Record<K, V>>* buckets;
    char pad[4];
};

template<typename K, typename V>
Dict<K, V> MakeMap(int64 num_buckets) {
    Dict<K, V> dict;
    typedef Slice<Record<K, V>> record_slice_t;
    dict.buckets = phanaged(record_slice_t, (size_t) num_buckets);
    for (int i = 0; i < num_buckets; ++i) {
        dict.buckets[i] = MakeSlice<Record<K,V>>(4);
    }
    dict.num_buckets = num_buckets;
    return dict;
}

template<typename K, typename V>
void insert(Dict<K, V>* dict, K key, V value) {
    uint64_t n = (hash(key)) % dict->num_buckets;
    Record<K, V> record;
    record.key = phanaged(K, 1);
    memcpy(record.key, &key, sizeof(key));
    record.value = value;
    append(&dict->buckets[n], record);
}

template<typename K, typename V>
V find(Dict<K, V>* dict, K key) {
    uint64_t n = hash(key) % dict->num_buckets;
    int64 i = 0;
    while (*(dict->buckets[n][i].key) != key) {
        i++;
    }
    return dict->buckets[n][i].value;
}

}
