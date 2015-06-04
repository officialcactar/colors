/* See LICENSE file for copyright and license details. */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include <png.h>
#include "colors.h"

void
parseimg(char *f, void (*fn)(int, int, int))
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

	rows = malloc(sizeof(*rows) * height);
	if (!rows)
		err(1, "malloc");
	for (y = 0; y < height; y++) {
		rows[y] = malloc(png_get_rowbytes(png_ptr, info_ptr));
		if (!rows[y])
			err(1, "malloc");
	}

	png_read_image(png_ptr, rows);

	for (y = 0; y < height; y++) {
		png_byte *row = rows[y];
		for (x = 0; x < width; x++) {
			png_byte *p = &row[x * 4];
			fn(p[0], p[1], p[2]);
		}
	}

	for (y = 0; y < height; y++)
		free(rows[y]);
	free(rows);

	fclose(fp);

}
