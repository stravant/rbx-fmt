
#pragma once

#include <stdint.h>

struct hash_entry {
	uint8_t *key;
	void *value;
	hash_entry *next;
};

struct hash_table {
	struct hash_entry **buckets;
	size_t bucket_count;
};

/* Construct / Destroy */
void hash_init(struct hash_table *tb);
void hash_destroy(struct hash_table *tb);

/* Allocate / Free helpers */
struct hash_table *hash_new();
void hash_free(struct hash_table *tb);

/* Get / Set entries */
void *hash_get(struct hash_table *tb, const char *key);
void hash_set(struct hash_table *tb, const char *key, void *value);

/* Count */
size_t hash_count(struct hash_table *tb);

/* Contains */
int hash_contains(struct hash_table *tb, const char *key);

/* Iterate */
const char *hash_next(struct hash_table *tb);

