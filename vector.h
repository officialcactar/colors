struct vector_data {
	void *raw;
};

struct vector {
	struct vector_data *d;
	size_t c;
	size_t s;
};

void vector_init(struct vector *);
void vector_free(struct vector *);
void vector_add(struct vector *, void *);
void *vector_get(struct vector *, size_t);
size_t vector_size(struct vector *);
