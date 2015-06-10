/* See LICENSE file for copyright and license details. */
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arg.h"
#include "colors.h"
#include "queue.h"
#include "vector.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

struct point {
	int x, y, z;
	struct cluster *c;
	TAILQ_ENTRY(point) e;
};

struct cluster {
	struct point center;
	TAILQ_HEAD(members, point) members;
	size_t nmembers;
};

char *argv0;

struct cluster *clusters;
size_t nclusters = 8;
struct vector points;

int eflag;
int rflag;
int hflag;
int pflag;
int mflag;

int
distance(struct point *p1, struct point *p2)
{
	int dx, dy, dz;

	if (mflag) {
		dx = abs(p1->x - p2->x);
		dy = abs(p1->y - p2->y);
		dz = abs(p1->z - p2->z);
	} else {
		dx = (p1->x - p2->x) * (p1->x - p2->x);
		dy = (p1->y - p2->y) * (p1->y - p2->y);
		dz = (p1->z - p2->z) * (p1->z - p2->z);
	}
	return dx + dy + dz;
}

void
adjmeans(struct cluster *c)
{
	struct point *p;
	struct point newc = { 0 };
	long x, y, z;

	if (!c->nmembers)
		return;

	x = y = z = 0;
	TAILQ_FOREACH(p, &c->members, e) {
		x += p->x;
		y += p->y;
		z += p->z;
	}
	newc.x = x / c->nmembers;
	newc.y = y / c->nmembers;
	newc.z = z / c->nmembers;
	c->center = newc;
}

struct cluster *curcluster;
int
pointcmp(const void *a, const void *b)
{
	struct point *p1 = *(struct point **)a;
	struct point *p2 = *(struct point **)b;
	int d1, d2;

	d1 = distance(&curcluster->center, p1);
	d2 = distance(&curcluster->center, p2);
	return d1 - d2;
}

void
adjmedians(struct cluster *c)
{
	struct point *p, **tab;
	struct point newc = { 0 };
	long x, y, z;
	size_t i;

	if (!c->nmembers)
		return;

	/* create a table out of the list to make sorting easy */
	tab = malloc(c->nmembers * sizeof(*tab));
	if (!tab)
		err(1, "malloc");
	i = 0;
	TAILQ_FOREACH(p, &c->members, e)
		tab[i++] = p;

	qsort(tab, c->nmembers, sizeof(*tab), pointcmp);

	/* calculate median */
	x = tab[c->nmembers / 2]->x;
	y = tab[c->nmembers / 2]->y;
	z = tab[c->nmembers / 2]->z;
	if (!(c->nmembers % 2)) {
		x += tab[c->nmembers / 2 - 1]->x;
		y += tab[c->nmembers / 2 - 1]->y;
		z += tab[c->nmembers / 2 - 1]->z;
		newc.x = x / 2;
		newc.y = y / 2;
		newc.z = z / 2;
	} else {
		newc.x = x;
		newc.y = y;
		newc.z = z;
	}

	c->center = newc;
	free(tab);
}

void (*adjcluster)(struct cluster *c) = adjmeans;

void
adjclusters(struct cluster *c, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		curcluster = &c[i];
		adjcluster(&c[i]);
	}
}

void
initcluster_greyscale(struct cluster *c, int i)
{
	TAILQ_INIT(&c->members);
	c->nmembers = 0;
	c->center.x = i;
	c->center.y = i;
	c->center.z = i;
}

void
initcluster_pixel(struct cluster *c, int i)
{
	struct point *p;

	TAILQ_INIT(&c->members);
	c->nmembers = 0;
	p = vector_get(&points, i);
	c->center = *p;
}

struct hue {
	int rgb[3];
	int i; /* index in rgb[] of color to change next */
} huetab[] = {
	{ { 0xff, 0x00, 0x00 }, 2 }, /* red */
	{ { 0xff, 0x00, 0xff }, 0 }, /* purple */
	{ { 0x00, 0x00, 0xff }, 1 }, /* blue */
	{ { 0x00, 0xff, 0xff }, 2 }, /* cyan */
	{ { 0x00, 0xff, 0x00 }, 0 }, /* green */
	{ { 0xff, 0xff, 0x00 }, 1 }, /* yellow */
};

struct point
hueselect(int i)
{
	struct point p = { 0 };
	struct hue h;
	int idx, mod;

	idx = i / 256;
	mod = i % 256;
	h = huetab[idx];

	switch (h.rgb[h.i]) {
	case 0x00:
		h.rgb[h.i] += mod;
		break;
	case 0xff:
		h.rgb[h.i] -= mod;
		break;
	}
	p.x = h.rgb[0];
	p.y = h.rgb[1];
	p.z = h.rgb[2];
	return p;
}

void
initcluster_hue(struct cluster *c, int i)
{
	TAILQ_INIT(&c->members);
	c->nmembers = 0;
	c->center = hueselect(i);
}

void (*initcluster)(struct cluster *c, int i) = initcluster_greyscale;
size_t initspace = 256;

void
initclusters(struct cluster *c, size_t n)
{
	size_t i, next;
	size_t step = initspace / n;

	clusters = malloc(sizeof(*clusters) * n);
	if (!clusters)
		err(1, "malloc");
	for (i = 0; i < n; i++) {
		next = rflag ? rand() % initspace : i * step;
		initcluster(&clusters[i], next);
	}
}

void
addmember(struct cluster *c, struct point *p)
{
	TAILQ_INSERT_TAIL(&c->members, p, e);
	p->c = c;
	c->nmembers++;
}

void
delmember(struct cluster *c, struct point *p)
{
	TAILQ_REMOVE(&c->members, p, e);
	p->c = NULL;
	c->nmembers--;
}

int
hasmember(struct cluster *c, struct point *p)
{
	return p->c == c;
}

void
process(void)
{
	struct point *p;
	int *dists, mind, mini, i, j, done = 0;

	dists = malloc(nclusters * sizeof(*dists));
	if (!dists)
		err(1, "malloc");

	while (!done) {
		done = 1;
		for (j = 0; j < vector_size(&points); j++) {
			p = vector_get(&points, j);
			for (i = 0; i < nclusters; i++)
				dists[i] = distance(p, &clusters[i].center);

			/* find the cluster that is nearest to the point */
			mind = dists[0];
			mini = 0;
			for (i = 1; i < nclusters; i++) {
				if (mind > dists[i]) {
					mind = dists[i];
					mini = i;
				}
			}

			if (hasmember(&clusters[mini], p))
				continue;

			/* not done yet, move point to nearest cluster */
			done = 0;
			for (i = 0; i < nclusters; i++) {
				if (hasmember(&clusters[i], p)) {
					delmember(&clusters[i], p);
					break;
				}
			}
			addmember(&clusters[mini], p);
		}
		adjclusters(clusters, nclusters);
	}
}

void
fillpoints(int r, int g, int b)
{
	struct point *p;

	p = malloc(sizeof(*p));
	if (!p)
		err(1, "malloc");
	p->x = r;
	p->y = g;
	p->z = b;
	p->c = NULL;
	vector_add(&points, p);
}

void
printclusters(void)
{
	int i;

	for (i = 0; i < nclusters; i++)
		if (clusters[i].nmembers || eflag)
			printf("#%02x%02x%02x\n",
			       clusters[i].center.x,
			       clusters[i].center.y,
			       clusters[i].center.z);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-emr] [-h | -p] [-n clusters] file\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *e;

	ARGBEGIN {
	case 'e':
		eflag = 1;
		break;
	case 'm':
		mflag = 1;
		break;
	case 'r':
		rflag = 1;
		break;
	case 'h':
		hflag = 1;
		pflag = 0;
		break;
	case 'p':
		pflag = 1;
		hflag = 0;
		break;
	case 'n':
		errno = 0;
		nclusters = strtol(EARGF(usage()), &e, 10);
		if (*e || errno || !nclusters)
			errx(1, "invalid number");
		break;
	default:
		usage();
	} ARGEND;

	if (argc != 1)
		usage();

	vector_init(&points);
	parseimg(argv[0], fillpoints);

	if (mflag)
		adjcluster = adjmedians;
	if (rflag)
		srand(time(NULL));
	if (pflag) {
		initcluster = initcluster_pixel;
		initspace = vector_size(&points);
	}
	if (hflag) {
		initcluster = initcluster_hue;
		initspace = LEN(huetab) * 256;
	}
	/* cap number of clusters */
	if (nclusters > initspace)
		nclusters = initspace;

	initclusters(clusters, nclusters);
	process();
	printclusters();
	return 0;
}
