/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

struct color_t {
	char   hex[8];
	int    rgb[3];
	double val;
	struct color_t *next;
};

struct color_t *head = NULL;
struct color_t **sorted = NULL;
struct color_t **messed = NULL;

void
usage(char *argv0) {
	fprintf(stderr, "usage: %s [kbgcrmyw]\n", argv0);
}

/*
 * converts an hexadecimal representation of a color into a 3 dimensionnal
 * array (RGB decomposition)
 */
void
hex2rgb(char *hex, int *rgb)
{
	int i;
	char tmp[2];
	for (i = 0; i < 3; i++) {
		strncpy(tmp, hex + 1 + 2 * i, 2);
		rgb[i] = strtol(tmp, NULL, 16);
	}
}

/*
 * Calculate a score for the dominance of the color given by `mask` (3 bit
 * representation of the RGB color, eg: 101 is magenta).
 * For now, it sums the 1 bits, calculate the average value and substract the
 * negative value of the color (eg, opposite of 101 is 010: green).
 * TODO: take into consideration the "distance" between two colors for two
 * bits masks.
 * For example, with mask 101:
 * 
 * HECODE   RGB      SCORE
 * -----------------------
 * #500050  80,0,80  80
 * #ff0000  255,0,0  127.5
 *
 * #ff0000 gets a higher score because it's brigther. But that's definitely not
 * a nice magenta.
 */
double
color_dominant(int *rgb, uint8_t mask)
{
	double pure = 0, negative = 0;

	/* get the score of the current value */
	pure     += rgb[0] * ((mask & 4)>>2);
	pure     += rgb[1] * ((mask & 2)>>1);
	pure     += rgb[2] * ((mask & 1)>>0);

	/* score of the negative value */
	negative += rgb[0] * (((mask ^ 4)>>2)&1);
	negative += rgb[1] * (((mask ^ 2)>>1)&1);
	negative += rgb[2] * (((mask ^ 1)>>0)&1);

	/*
	 * calculate the average of either pure or negative color depending on
	 * the mask's value
	 */
	switch (mask) {
		case 1:
		case 2:
		case 4:
			negative /= 2;
			break;
		case 3:
		case 5:
		case 6:
			pure /= 2;
			break;
		case 7:
			return pure;
		case 0:
			return -negative;
	}

	/* return our color's ratio */
	return negative > 0 ? pure / negative : pure;
}

/* create a color node, and add it to the list */
struct color_t *
color_new(char *hex, uint8_t mask)
{
	struct color_t *new = NULL;

	new = malloc(sizeof(struct color_t));
	if (new == NULL)
		return;

	strncpy(new->hex, hex, 8);
	hex2rgb(hex, new->rgb);
	new->val = color_dominant(new->rgb, mask); 
	return new;
}

/*
 * Takes an unsorted list of colors as an argument, and sort it depending on
 * the mask value.
 * The mask is a 3 bit representation of the RGB composition you want to use to
 * sort colors, eg mask 011 will return the brightess cyan first, and darkest
 * red last.
 */
struct color_t *
color_sort(struct color_t *cur, struct color_t *new, uint8_t mask)
{
	int newval;
	if (cur == NULL) {
		new->next = NULL;
	} else {
		if (new->val <= cur->val) {
			cur->next = color_sort(cur->next, new, mask);
			return cur;
		} else {
			new->next = cur;
		}
	}
	return new;
}

/*
 * print the content of our list in the format:
 * <HEX>	<RGB>	<SCORE>
 */
void
color_print(struct color_t *node)
{
	struct color_t *tmp = NULL;
	for (tmp=node; tmp; tmp=tmp->next) {
		printf("%s\t", tmp->hex);
		printf("%d,%d,%d\t", tmp->rgb[0], tmp->rgb[1], tmp->rgb[2]);
		printf("%f\n", tmp->val);
	}
}

int
main(int argc, char *argv[])
{
	int rgb[3];
	char hex[8];
	uint8_t mask = 7;

	/* print whitest by default */
	if (argc > 1) {
		switch (argv[1][0]) {
			case 'k': mask = 0; break;
			case 'b': mask = 1; break;
			case 'g': mask = 2; break;
			case 'c': mask = 3; break;
			case 'r': mask = 4; break;
			case 'm': mask = 5; break;
			case 'y': mask = 6; break;
			case 'w': mask = 7; break;
			default:
				  usage(argv[0]);
				  return 1;
		}
	}

	while (fgets(hex, 8, stdin)) {
		if (hex[0] == '#') {
			head = color_sort(head, color_new(hex, mask), mask);
		}
	}
	color_print(head);
	return 0;
}
