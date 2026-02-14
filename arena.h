/*
 *  arena.h - Header-only implementation of arena allocator
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

#ifndef __ARENA_H
#define __ARENA_H

#ifndef ARENA_NO_STDIO
#include <stdio.h>
#define _arena_fprintf fprintf
#else
#define _arena_fprintf(...)
#endif /* ARENA_NO_STDIO */

#include <stddef.h>
#include <string.h>

#ifndef ARENADEF
#define ARENADEF static inline
#endif /* ARENADEF */

#ifdef ARENA_MMAP_BACKEND                   /* use mmap/munmap for portability (and speed) */
#include <sys/mman.h>
#define _ARENA_INVALID_ALLOC                MAP_FAILED
#define _ARENA_BACKEND_ALLOC(size)          mmap(NULL, (size), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
#define _ARENA_BACKEND_DEALLOC(addr, size)  munmap((addr), (size))

#else                                       /* defaults to malloc/free */

#include <stdlib.h>
#define _ARENA_INVALID_ALLOC                NULL
#define _ARENA_BACKEND_ALLOC(size)          malloc((size))
#define _ARENA_BACKEND_DEALLOC(addr, size)  free((addr))
#endif /* ARENA_MMAP_BACKEND */

/* 4KB is the most common page size, so by default the arena will allocate 2 pages on most systems */
#define ARENA_DEFAULT_CAPACITY             (2 * 4096)

struct _memory_region {
    size_t                  size;
    size_t                  offset;
    struct _memory_region   *next;
    char                    mem[];
};

struct _arena_alloc_header {
    size_t  size;
    char    mem[];
};

typedef struct _arena {
    size_t                  total;          /* total bytes used by the arena */
    struct _memory_region   *head;
    struct _memory_region   *tail;
} s_arena, p_arena[1];

ARENADEF struct _memory_region *_alloc_memory_region(size_t size, struct _memory_region *next) {
    struct _memory_region *region = _ARENA_INVALID_ALLOC;

    if ((region = _ARENA_BACKEND_ALLOC(sizeof(*region) + size)) != _ARENA_INVALID_ALLOC) {
        region->next = next;
        region->size = size;
        region->offset = 0;
        return region;
    }

    _arena_fprintf(stderr, "%s:%d: Failed to allocate %lu bytes\n", __FILE__, __LINE__, sizeof(*region) + size);
    abort();
}

ARENADEF void _arena_append_region(p_arena arena, size_t size) {
    struct _memory_region *region = _ARENA_INVALID_ALLOC;

    if ((region = _ARENA_BACKEND_ALLOC(sizeof(*region) + size)) == _ARENA_INVALID_ALLOC) {
        _arena_fprintf(stderr, "%s:%d: Failed to allocate %lu bytes\n", __FILE__, __LINE__, sizeof(*region) + size);
        abort();
    }

    region->next = NULL;
    region->size = size;
    region->offset = 0;

    if (arena->tail) arena->tail->next = region;
    if (!arena->head) arena->head = region;

    arena->tail = region;
    arena->total += sizeof(*arena->tail) + arena->tail->size;
}

ARENADEF void arena_init(p_arena arena) {
    arena->total = 0;
    _arena_append_region(arena, ARENA_DEFAULT_CAPACITY);
}

ARENADEF void arena_deinit(p_arena arena) {
    struct _memory_region *next = arena->head;
    while (next != NULL) {
        struct _memory_region *region = next;
        next = region->next;
        _ARENA_BACKEND_DEALLOC(region, sizeof(*region) + region->size);
    }
    arena->total = 0;
}

ARENADEF void *arena_alloc(p_arena arena, size_t size) {
    if (arena->tail == NULL) arena_init(arena);

    size_t required = sizeof(struct _arena_alloc_header) + size;
    struct _memory_region *region = arena->head;

    while (region) {
        if (required <= region->size - region->offset) break;
        region = region->next;
    }

    if (region == NULL) {
        _arena_append_region(arena, required > ARENA_DEFAULT_CAPACITY ? required : ARENA_DEFAULT_CAPACITY);
        region = arena->tail;
    }

    struct _arena_alloc_header *header = (void *)((char *)region->mem + region->offset);
    header->size = size;
    region->offset += required;

    return header->mem;
}

ARENADEF struct _memory_region * _arena_find_region(p_arena arena, void *ptr) {
    struct _arena_alloc_header *header = (void *)((char *)ptr - sizeof(*header));
    char *endptr = header->mem + header->size;
    struct _memory_region *region = arena->head;
    while (region) {
        if ((char *)ptr >= region->mem && endptr <= region->mem + region->offset) {
            return region;
        }
        region = region->next;
    }
    _arena_fprintf(stderr, "%s:%d: Memory at address %p was not allocated by this arena\n", __FILE__, __LINE__, ptr);
    abort();
}

ARENADEF void arena_free(p_arena arena, void *ptr) {
    if (!ptr) return;
    struct _arena_alloc_header *header = (void *)((char *)ptr - sizeof(*header));
    struct _memory_region *region = _arena_find_region(arena, ptr);
    if (header->mem + header->size == region->mem + region->offset) {
        region->offset -= header->size - sizeof(*header);
    }
}

ARENADEF void *arena_realloc(p_arena arena, void *ptr, size_t size) {
    if (ptr == NULL) return arena_alloc(arena, size);

    struct _arena_alloc_header *header = (void *)((char *)ptr - sizeof(*header));

    if (size <= header->size) {
        header->size = size;
        return header->mem;
    }

    size_t required = size - header->size;
    char *endptr = header->mem + header->size;

    struct _memory_region *region = _arena_find_region(arena, ptr);
    if (endptr == region->mem + region->offset && required <= (region->size - region->offset)) {
        header->size = size;
        region->offset += required;
        return header->mem;
    }

    return memcpy(arena_alloc(arena, size), header->mem, header->size);
}

#define arena_memclone(arena, ptr, size) memcpy(arena_alloc((arena), (size)), (ptr), (size))

#endif /* arena.h */
