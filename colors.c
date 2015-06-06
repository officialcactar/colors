/* See LICENSE file for copyright and license details. */
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arg.h"
#include "colors.h"
#include "queue.h"

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
TAILQ_HEAD(points, point) points;
size_t npoints;
int eflag;
int rflag;
int hflag;

int
distance(struct point *p1, struct point *p2)
{
	int dx, dy, dz;

	dx = (p1->x - p2->x) * (p1->x - p2->x);
	dy = (p1->y - p2->y) * (p1->y - p2->y);
	dz = (p1->z - p2->z) * (p1->z - p2->z);
	return sqrtf(dx + dy + dz) + 0.5f;
}

void
adjcluster(struct cluster *c)
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

void
adjclusters(struct cluster *c, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		adjcluster(&c[i]);
}

void
initcluster_brightness(struct cluster *c, int i)
{
	TAILQ_INIT(&c->members);
	c->nmembers = 0;
	c->center.x = i;
	c->center.y = i;
	c->center.z = i;
}

void
initcluster_rand(struct cluster *c, int unused)
{
	struct point *p;
	int i, sel;

	TAILQ_INIT(&c->members);
	c->nmembers = 0;
	sel = rand() % npoints;
	i = 0;
	TAILQ_FOREACH(p, &points, e)
		if (i++ == sel)
			break;
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
	struct point p;
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

void (*initcluster)(struct cluster *c, int i) = initcluster_brightness;
size_t initspace = 256;

void
initclusters(struct cluster *c, size_t n)
{
	size_t i;
	size_t step = initspace / n;

	clusters = malloc(sizeof(*clusters) * n);
	if (!clusters)
		err(1, "malloc");
	for (i = 0; i < n; i++)
		initcluster(&clusters[i], i * step);
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
	struct point *p, *tmp;
	int *dists, mind, mini, i, done = 0;

	dists = malloc(nclusters * sizeof(*dists));
	if (!dists)
		err(1, "malloc");

	while (!done) {
		done = 1;
		TAILQ_FOREACH_SAFE(p, &points, e, tmp) {
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
	TAILQ_INSERT_TAIL(&points, p, e);
	npoints++;
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
	fprintf(stderr, "usage: %s [-erh] [-n clusters] file\n", argv0);
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
	case 'r':
		rflag = 1;
		break;
	case 'h':
		hflag = 1;
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

	if (rflag) {
		srand(time(NULL));
		initcluster = initcluster_rand;
		initspace = 256 * 256 * 256;
	}
	if (hflag) {
		initcluster = initcluster_hue;
		initspace = LEN(huetab) * 256;
	}
	/* cap number of clusters */
	if (nclusters > initspace)
		nclusters = initspace;

	TAILQ_INIT(&points);
	parseimg(argv[0], fillpoints);
	initclusters(clusters, nclusters);
	process();
	printclusters();
	return 0;
}
