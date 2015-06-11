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
#include "tree.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

struct point {
	int x;
	int y;
	int z;
	int freq;
	struct cluster *c;
	TAILQ_ENTRY(point) e;
	RB_ENTRY(point) rb_e;
};

struct cluster {
	struct point center;
	TAILQ_HEAD(pointhead, point) pointhead;
};

char *argv0;

struct cluster *clusters;
size_t nclusters = 8;
RB_HEAD(pointtree, point) pointhead;
size_t npoints;

int eflag;
int rflag;
int hflag;
int pflag;

int
distance(struct point *p1, struct point *p2)
{
	int dx, dy, dz;

	dx = (p1->x - p2->x) * (p1->x - p2->x);
	dy = (p1->y - p2->y) * (p1->y - p2->y);
	dz = (p1->z - p2->z) * (p1->z - p2->z);
	return dx + dy + dz;
}

int
pointcmp(struct point *p1, struct point *p2)
{
	struct point center = { 0 };

	return distance(&center, p1) - distance(&center, p2);
}

RB_GENERATE(pointtree, point, rb_e, pointcmp)

void
adjmeans(struct cluster *c)
{
	struct point *p;
	struct point newc = { 0 };
	size_t nmembers = 0;
	long x, y, z;

	if (TAILQ_EMPTY(&c->pointhead))
		return;

	x = y = z = 0;
	TAILQ_FOREACH(p, &c->pointhead, e) {
		nmembers += p->freq;
		x += p->x * p->freq;
		y += p->y * p->freq;
		z += p->z * p->freq;
	}
	newc.x = x / nmembers;
	newc.y = y / nmembers;
	newc.z = z / nmembers;
	c->center = newc;
}

void
adjclusters(struct cluster *c, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		adjmeans(&c[i]);
}

void
initcluster_greyscale(struct cluster *c, int i)
{
	TAILQ_INIT(&c->pointhead);
	c->center.x = i;
	c->center.y = i;
	c->center.z = i;
}

void
initcluster_pixel(struct cluster *c, int i)
{
	struct point *p;

	TAILQ_INIT(&c->pointhead);
	RB_FOREACH(p, pointtree, &pointhead)
		if (i-- == 0)
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
	TAILQ_INIT(&c->pointhead);
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
	TAILQ_INSERT_TAIL(&c->pointhead, p, e);
	p->c = c;
}

void
delmember(struct cluster *c, struct point *p)
{
	TAILQ_REMOVE(&c->pointhead, p, e);
	p->c = NULL;
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
	int *dists, mind, mini, i, done = 0;

	dists = malloc(nclusters * sizeof(*dists));
	if (!dists)
		err(1, "malloc");

	while (!done) {
		done = 1;
		RB_FOREACH(p, pointtree, &pointhead) {
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
	struct point n = { 0 };
	struct point *p;

	n.x = r, n.y = g, n.z = b;
	p = RB_FIND(pointtree, &pointhead, &n);
	if (p) {
		p->freq++;
		return;
	}

	p = malloc(sizeof(*p));
	if (!p)
		err(1, "malloc");
	p->x = r;
	p->y = g;
	p->z = b;
	p->freq = 1;
	p->c = NULL;
	npoints++;
	RB_INSERT(pointtree, &pointhead, p);
}

void
printclusters(void)
{
	int i;

	for (i = 0; i < nclusters; i++)
		if (!TAILQ_EMPTY(&clusters[i].pointhead) || eflag)
			printf("#%02x%02x%02x\n",
			       clusters[i].center.x,
			       clusters[i].center.y,
			       clusters[i].center.z);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-er] [-h | -p] [-n clusters] file\n", argv0);
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

	RB_INIT(&pointhead);
	parseimg(argv[0], fillpoints);

	if (rflag)
		srand(time(NULL));
	if (pflag) {
		initcluster = initcluster_pixel;
		initspace = npoints;
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
