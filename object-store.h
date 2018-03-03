#ifndef OBJECT_STORE_H
#define OBJECT_STORE_H

struct raw_object_store {
	/*
	 * Path to the repository's object store.
	 * Cannot be NULL after initialization.
	 */
	char *objectdir;

	/* Path to extra alternate object database if not NULL */
	char *alternate_db;
};

void raw_object_store_init(struct raw_object_store *o);
void raw_object_store_clear(struct raw_object_store *o);

#endif /* OBJECT_STORE_H */
