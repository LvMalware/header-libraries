/*
 *  hashmap.h - Header-only implementation of dynamic hash table/map data structure
 *  Copyright (C) 2025  Lucas V. Araujo <root@lva.sh>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __HASHMAP_H
#define __HASHMAP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* initial size of the hashmap, set as a power of 2 for convenience. */
#define HASHMAP_CAP_DEFAULT ((size_t)8)
#define HASHMAP_CAP_MASK(hm) ((hm).capacity - 1)

typedef struct {
    uint8_t *key;                /* pointer to the key bytes */ 
    uint32_t len;                /* number of bytes in the key */ 
    uint8_t used;                /* set to 1 if this entry is being used */
} s_hashmap_meta;

#define MAKE_HASHMAP(type) \
    size_t count;                /* number of occupied entries */ \
    ssize_t index;               /* used when searching for a key */ \
    size_t capacity;             /* Total capacity of the hashmap, will grow as needed */ \
    uint32_t maxcol;             /* the maximum number of collisions we found, used as a higher limit on lookup */ \
    struct { \
        s_hashmap_meta meta; /*  metadata of the entry */\
        type data;               /* the actual data that this entry holds */ \
    } *items

static inline size_t hashmap_hash(const uint8_t *str, uint32_t len) {
    /* FNV-1 hash */
    size_t i, hash = 0xcbf29ce484222325;
    for (i = 0; i < len; i ++) {
        hash *= 0x100000001b3;
        hash ^= str[i];
    }
    return hash;
}

static inline ssize_t hashmap_lookup(const void *items, size_t itemlen, size_t capacity, uint32_t maxcol, const uint8_t *str, uint32_t len) {
    if (!items) return -1;
    size_t mask = capacity - 1;
    size_t index = hashmap_hash(str, len) & mask;
    uint32_t col = 0;
    while (col <= maxcol) {
        const s_hashmap_meta *meta = (void *)((size_t)items + index * itemlen);
        if (meta->used && meta->key[0] == str[0] && meta->len == len && memcmp(str, meta->key, len) == 0) return index;
        col ++;
        index = (index + 1) & mask;
    }
    return -1;
}


#define hashmap_init_cap(hm, cap) do { \
    (hm).items = calloc((cap), sizeof(*(hm).items)); \
    if ((hm).items == NULL) { \
        fprintf(stderr, "%s:%d: Failed to init hashmap: calloc() failed to allocate %lu bytes\n", __FILE__, __LINE__, (size_t)(cap)); \
        abort(); \
    } \
    (hm).count = 0; \
    (hm).maxcol = 0; \
    (hm).capacity = (cap); \
} while (0)

#define hashmap_deinit(hm) do { \
    (hm).count = 0; \
    (hm).capacity = 0; \
    free((hm).items); \
    (hm).items = NULL; \
} while (0)

#define hashmap_at(hm, index) (hm).items[(index)].data
#define hashmap_get(hm, kstr, klen) ((hm).index = hashmap_lookup((hm).items, sizeof(*(hm).items), (hm).capacity, (hm).maxcol, (uint8_t *)(kstr), (klen)), (hm).items[(hm).index].data)
#define hashmap_init(hm) hashmap_init_cap(hm, HASHMAP_CAP_DEFAULT) 
#define hashmap_avail(hm) ((hm).capacity - (hm).count)
#define hashmap_index(hm, kstr, klen) ((hm).index = hashmap_lookup((hm).items, sizeof(*(hm).items), (hm).capacity, (hm).maxcol, (uint8_t *)(kstr), (klen)), (hm).index)
#define hashmap_contains(hm, kstr, klen) ((hm).index = hashmap_lookup((hm).items, sizeof(*(hm).items), (hm).capacity, (hm).maxcol, (uint8_t *)(kstr), (klen)), (hm).index >= 0)
#define hashmap_match_item(item, kbuf, klen) ((kbuf)[0] == (item).meta.key[0] && klen == (item).meta.len && memcmp((kbuf), (item).meta.key, (klen)) == 0)

#define hashmap_put_nogrow(hm, value, kbuf, klen) do { \
    if ((hm).count >= (hm).capacity) abort(); /* This should not happen */ \
    uint32_t __hashmap_put_nogrow_count = 0; \
    size_t __hashmap_put_nogrow_mask = HASHMAP_CAP_MASK(hm); \
    size_t __hashmap_put_nogrow_index = hashmap_hash((uint8_t *)(kbuf), (uint32_t)(klen)) & __hashmap_put_nogrow_mask; \
    while ((hm).items[__hashmap_put_nogrow_index].meta.used) { \
        if (hashmap_match_item((hm).items[__hashmap_put_nogrow_index], (kbuf), (klen))) break; \
        __hashmap_put_nogrow_count += 1; \
        __hashmap_put_nogrow_index = (__hashmap_put_nogrow_index + 1) & __hashmap_put_nogrow_mask; \
    } \
    (hm).items[__hashmap_put_nogrow_index].data = value; \
    if ((hm).items[__hashmap_put_nogrow_index].meta.used) break; \
    (hm).items[__hashmap_put_nogrow_index].meta.len = (uint32_t)klen; \
    (hm).items[__hashmap_put_nogrow_index].meta.key = (uint8_t *)kbuf; \
    (hm).items[__hashmap_put_nogrow_index].meta.used = 1; \
    (hm).count ++; \
    if ((hm).maxcol < __hashmap_put_nogrow_count) (hm).maxcol = __hashmap_put_nogrow_count; \
} while (0)

#define hashmap_grow(hm) do { \
    __typeof__((hm)) __hashmap_grow_tmp = { 0 }; \
    hashmap_init_cap(__hashmap_grow_tmp, (hm).capacity << 1); \
    size_t __hashmap_grow_index; \
    for (__hashmap_grow_index = 0; __hashmap_grow_index < (hm).capacity; __hashmap_grow_index ++) { \
        if ((hm).items[__hashmap_grow_index].meta.used) { \
            hashmap_put_nogrow(__hashmap_grow_tmp, (hm).items[__hashmap_grow_index].data, (hm).items[__hashmap_grow_index].meta.key, (hm).items[__hashmap_grow_index].meta.len); \
        } \
    } \
    hashmap_deinit((hm)); \
    (hm).items = __hashmap_grow_tmp.items; \
    (hm).count = __hashmap_grow_tmp.count; \
    (hm).maxcol = __hashmap_grow_tmp.maxcol; \
    (hm).capacity = __hashmap_grow_tmp.capacity; \
} while (0)

#define hashmap_shrink(hm) do { \
    /* do not shrink if the current occupation is more than 1/4 of the capacity */ \
    if ((hm).count > (hm).capacity / 4) break; \
    __typeof__((hm)) __hashmap_shrink_tmp = { 0 }; \
    hashmap_init_cap(__hashmap_shrink_tmp, (hm).capacity >> 1); \
    size_t __hashmap_shrink_index; \
    for (__hashmap_shrink_index = 0; __hashmap_shrink_index < (hm).count; __hashmap_shrink_index ++) { \
        if ((hm).items[__hashmap_shrink_index].meta.used) { \
            hashmap_put_nogrow(__hashmap_shrink_tmp, (hm).items[__hashmap_shrink_index].data, (hm).items[__hashmap_shrink_index].meta.key, (hm).items[__hashmap_shrink_index].meta.len); \
        } \
    } \
    hashmap_deinit((hm)); \
    (hm).items = __hashmap_shrink_tmp.items; \
    (hm).count = __hashmap_shrink_tmp.count; \
    (hm).maxcol = __hashmap_shrink_tmp.maxcol; \
    (hm).capacity = __hashmap_shrink_tmp.capacity; \
} while (0)

#define hashmap_put(hm, value, kbuf, klen) do { \
    /* calculate the load factor. If bigger than 0.7, we increase the hashmap's capacity and reinsert everything */ \
    if ((double)(hm).count / (double)(hm).capacity >= 0.7) { \
        hashmap_grow((hm)); \
    } \
    hashmap_put_nogrow((hm), (value), (kbuf), (klen)); \
} while (0)

#define hashmap_remove(hm, kbuf, klen) do { \
    /* this will leave a gap on the item buffer */ \
    ssize_t __hashmap_remove_index = -1; \
    hashmap_index_var((hm), __hashmap_remove_index, (kbuf), (klen)); \
    if (__hashmap_remove_index >= 0) { \
        (hm).items[__hashmap_remove_index].meta.used = 0; /* lazy removal */ \
        (hm).count --; \
    } \
} while (0)

#endif /* hashmap.h */
