/* only works in xterm, written by z3bra */

#include <stdio.h>
#include <string.h>

int
main (int argc, char **argv)
{
	char hexcode[8], shortcode[2];
	int i, rgb[3];

	while (read(0, hexcode, 8) > 0) {
		hexcode[7] = 0;
		for (i=0; i<3; i++) {
			strncpy(shortcode, hexcode + 1 + 2*i, 2);
			rgb[i] = strtol(shortcode, NULL, 16);
		}
		printf("[48;2;%d;%d;%dm%8s[0m ", rgb[0],rgb[1],rgb[2], "");
		printf("[38;2;%d;%d;%dm%s[0m\n", rgb[0],rgb[1],rgb[2], hexcode);
	}

	printf("[0m");
	return 0;
}
