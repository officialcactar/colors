#include <err.h>
#include <stdlib.h>

#include "vector.h"

static void
grow(struct vector *v)
{
	if (v->s >= v->c) {
		v->c = !v->c ? 1 : v->c * 2;
		v->d = realloc(v->d, sizeof(*v->d) * v->c);
		if (!v->d)
			err(1, "realloc");
	}
}

void
vector_init(struct vector *v)
{
	v->s = 0;
	v->c = 0;
	v->d = NULL;
}

void
vector_free(struct vector *v)
{
	free(v->d);
}

void
vector_add(struct vector *v, void *data)
{
	grow(v);
	v->d[v->s++].raw = data;
}

void *
vector_get(struct vector *v, size_t i)
{
	return v->d[i].raw;
}

size_t
vector_size(struct vector *v)
{
	return v->s;
}
