/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

//TC09: unclear condition

#include "aliascheck.h"

int sum(char *s) {
	char *p;
	int i = 0, j, r, n = (s == NULL ? 0 : strlen(s));
	if (n > 0) {
		p = (char *) PLKMALLOC(n);
		r = 0;
	} else
		r = -1;
	while (i < n) {
		j = 0;
		while ((s[i]) > 0)
			p[j++] = s[i++];
		if (j > 0) {
			p[j] = '\0';
			r += atoi(p);
		}
		i++;
	}
	if (r >= 0)
		free(p);
	return r;
}

int main(int argc, char *argv[]) {
	int i;
	for (i = 1; i < argc; i++)
		printf("%d\n", sum(argv[i]));
}

// false alarm!

