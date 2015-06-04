/* See LICENSE file for copyright and license details. */
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <png.h>
#include "arg.h"
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
size_t nclusters = 8;
TAILQ_HEAD(points, point) points;
size_t npoints;

void *
emalloc(size_t n)
{
	void *p;

	p = malloc(n);
	if (!p)
		err(1, "malloc");
	return p;
}

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
	struct point newc;

	if (!c->nmembers)
		return;

	newc.x = newc.y = newc.z = 0;
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

	clusters = emalloc(sizeof(*clusters) * n);
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

	dists = emalloc(nclusters * sizeof(*dists));
	while (!done) {
		done = 1;
		TAILQ_FOREACH_SAFE(p, &points, e, tmp) {
			/* calculate the distance of this point from all cluster centers */
			for (i = 0; i < nclusters; i++)
				dists[i] = distance(p, &clusters[i].c);

			/* find the cluster that is closest to the point */
			mind = dists[0];
			mini = 0;
			for (i = 1; i < nclusters; i++) {
				if (mind > dists[i]) {
					mind = dists[i];
					mini = i;
				}
			}

			/* if point is already part of the closest cluster, skip it */
			if (hasmember(&clusters[mini], p))
				continue;

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
printcolors(void)
{
	int i;

	for (i = 0; i < nclusters; i++)
		if (clusters[i].nmembers)
			printf("#%02x%02x%02x\n",
			       clusters[i].c.x,
			       clusters[i].c.y,
			       clusters[i].c.z);
}

void
initpoints(char *f)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte hdr[8];
	png_bytep *rows;
	int width, height;
	int x, y;

	if (!(fp = fopen(f, "r")))
		err(1, "fopen %s", f);
	fread(hdr, 1, 8, fp);
	if (png_sig_cmp(hdr, 0, 8))
		errx(1, "not a png file");

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);
	if (!png_ptr || !info_ptr || setjmp(png_jmpbuf(png_ptr)))
		errx(1, "failed to initialize libpng");

	png_init_io(png_ptr, fp);
	png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);
	png_set_gray_to_rgb(png_ptr);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	png_read_update_info(png_ptr, info_ptr);

	if (setjmp(png_jmpbuf(png_ptr)))
		errx(1, "failed to read image");

	rows = emalloc(sizeof(*rows) * height);
	for (y = 0; y < height; y++)
		rows[y] = emalloc(png_get_rowbytes(png_ptr, info_ptr));
	png_read_image(png_ptr, rows);

	for (y = 0; y < height; y++) {
		png_byte *row = rows[y];
		for (x = 0; x < width; x++) {
			png_byte *p = &row[x * 4];
			struct point *newp = emalloc(sizeof(*newp));
			newp->x = p[0];
			newp->y = p[1];
			newp->z = p[2];
			newp->c = NULL;
			TAILQ_INSERT_TAIL(&points, newp, e);
			npoints++;
		}
	}

	for (y = 0; y < height; y++)
		free(rows[y]);
	free(rows);

	fclose(fp);
}

void
printpoints(void)
{
	struct point *p;

	TAILQ_FOREACH(p, &points, e)
		printf("R: %02x, G: %02x, B: %02x\n", p->x, p->y, p->z);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-n clusters] png-file\n", argv0);
	fprintf(stderr, " -n\tset number of clusters, defaults to 8\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *e;

	ARGBEGIN {
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
	initpoints(argv[0]);
	initclusters(clusters, nclusters);
	process();
	printcolors();
	return 0;
}
