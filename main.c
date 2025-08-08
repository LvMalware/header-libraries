#include <stdio.h>
#include <assert.h>
#include "hashmap.h"

typedef struct {
    MAKE_HASHMAP(int);
} inthashmap;

int main() {
    inthashmap hm = { 0 };
    hashmap_init(hm);

    FILE *file = fopen("/usr/share/dict/words", "r");
    assert(file != NULL);

    int count = 1;
    while (!feof(file)) {
        char *word = NULL;
        size_t blen = 0;
        ssize_t len = getline(&word, &blen, file);
        if (len <= 0) continue;
        word[--len] = 0;
        hashmap_put(hm, count, word, len);
        count ++;
    }

    if (hashmap_contains(hm, "table", 5)) {
        printf("Index = %zd\n", hm.index);
        printf("Value = %d\n", hashmap_at(hm, hm.index));
    }

    printf("Value at key A = %d\n", hashmap_get(hm, "A", 1));

    hashmap_deinit(hm);
    return 0;
}
