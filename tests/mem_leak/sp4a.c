/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

//TC04: pointers in structure
#include "aliascheck.h"

typedef struct _S {
	char *p1;
	char *p2;
} S;

int main(int arg1) {
	S *p;
	char str1[10] = "STRING 1";
	char str2[10] = "STRING 2";

	p = (S*) SAFEMALLOC(sizeof(S));
	if (p == NULL)
		return -1;

	p->p1 = (char *) PLKLEAKFP(sizeof(char) * 10);
	if (p->p1 == NULL) {
		free(p);
		return -1;
	}

	p->p2 = (char *) NFRMALLOC(sizeof(char) * 10); //alarm
	if (p->p2 == NULL) {
		if (p->p1 != NULL)
			free(p->p1);
		free(p);
		return -1;
	}

	strcpy(p->p1, str1);
	strcpy(p->p2, str2);

	free(p->p1);
	//free(p->p2);
	free(p);
	return arg1;
}

