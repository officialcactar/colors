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

struct point {
	int x, y, z;
	struct cluster *c;
	TAILQ_ENTRY(point) e;
};

struct cluster {
	struct point c;
	TAILQ_HEAD(members, point) members;
	size_t nmembers;
};

char *argv0;
struct cluster *clusters;
size_t nclusters = 4;
TAILQ_HEAD(points, point) points;
size_t npoints;
int eflag;

int
distance(struct point *p1, struct point *p2)
{
	int dx, dy, dz;

	dx = (p1->x - p2->x) * (p1->x - p2->x);
	dy = (p1->y - p2->y) * (p1->y - p2->y);
	dz = (p1->z - p2->z) * (p1->z - p2->z);
	return sqrtf(dx + dy + dz);
}

void
adjcluster(struct cluster *c)
{
	struct point *p;
	struct point newc = { 0 };

	if (!c->nmembers)
		return;

	TAILQ_FOREACH(p, &c->members, e) {
		newc.x += p->x;
		newc.y += p->y;
		newc.z += p->z;
	}
	newc.x /= c->nmembers;
	newc.y /= c->nmembers;
	newc.z /= c->nmembers;
	c->c = newc;
}

void
adjclusters(struct cluster *c, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		adjcluster(&c[i]);
}

void
initcluster(struct cluster *c)
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
	c->c = *p;
}

void
initclusters(struct cluster *c, size_t n)
{
	size_t i;

	clusters = malloc(sizeof(*clusters) * n);
	if (!clusters)
		err(1, "malloc");
	for (i = 0; i < n; i++)
		initcluster(&clusters[i]);
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
			/* calculate the distance of this point from all cluster centers */
			for (i = 0; i < nclusters; i++)
				dists[i] = distance(p, &clusters[i].c);

			/* find the cluster that is nearest to the point */
			mind = dists[0];
			mini = 0;
			for (i = 1; i < nclusters; i++) {
				if (mind > dists[i]) {
					mind = dists[i];
					mini = i;
				}
			}

			/* if point is already part of the nearest cluster, skip it */
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
			       clusters[i].c.x,
			       clusters[i].c.y,
			       clusters[i].c.z);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-e] [-n clusters] file\n", argv0);
	fprintf(stderr, " -e\tinclude empty clusters\n");
	fprintf(stderr, " -n\tset number of clusters, defaults to 4\n");
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
	case 'n':
		errno = 0;
		nclusters = strtol(EARGF(usage()), &e, 10);
		if (*e || errno)
			errx(1, "invalid number");
		break;
	default:
		usage();
	} ARGEND;

	if (argc != 1)
		usage();

	srand(time(NULL));
	TAILQ_INIT(&points);
	parseimg(argv[0], fillpoints);
	initclusters(clusters, nclusters);
	process();
	printclusters();
	return 0;
}
