
#include "hash_table.c"

/* Perl's hash function */
static uint32_t hash_func(const void *key, size_t length) {
	register size_t i = length;
	register uint32_t hv = 0xBAADF00D;
	register const uint8_t *s = (uint8_t*)key;
	while (i--) {
		hv += *s++;
		hv += (hv << 10);
		hv ^= (hv >> 6);
	}
	hv += (hv << 3);
	hv ^= (hv >> 11);
	hv += (hv << 15);
	return hv;
}

