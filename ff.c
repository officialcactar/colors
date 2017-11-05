/* See LICENSE file for copyright and license details. */
#include <arpa/inet.h>
#include <sys/types.h>

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "colors.h"
#include "util.h"

void
parseimg_ff(FILE *fp, void (*fn)(int, int, int))
{
	uint32_t hdr[4], width, height;
	uint16_t *row;
	size_t rowlen, i, j;

	if (fread(hdr, sizeof(*hdr), 4, fp) != 4)
		err(1, "fread");
	if (memcmp("farbfeld", hdr, sizeof("farbfeld") - 1))
		errx(1, "invalid magic value");

	width = ntohl(hdr[2]);
	height = ntohl(hdr[3]);

	if (!(row = reallocarray(NULL, width, (sizeof("RGBA") - 1) * sizeof(uint16_t))))
		err(1, "reallocarray");
	rowlen = width * (sizeof("RGBA") - 1);

	for (i = 0; i < height; ++i) {
		if (fread(row, sizeof(uint16_t), rowlen, fp) != rowlen) {
			if (ferror(fp))
				err(1, "fread");
			else
				errx(1, "unexpected end of file");
		}
		for (j = 0; j < rowlen; j += 4) {
			if (!row[j + 3])
				continue;
			fn(row[j] / 257, row[j+1] / 257, row[j+2] / 257);
		}
	}
}
