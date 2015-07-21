/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

// TC15: List reversal 
struct List {
	struct List * next;
	int a;
};

struct List *reverse(struct List *x) {
	struct List *y, *t;
	y = x->next;
	free(x);
	x = y;
	while (x != 0) {
		t = x->next;
		x->next = y;
		y = x;
		x = t;
	}
	t = (struct List *) NFRMALLOC(sizeof(struct List*) * 1);
	t->next = y;
	return y;
}

int main() {
	struct List *node, *ret_val;
	node = (struct List *) SAFEMALLOC(sizeof(struct List*) * 1);
	ret_val = (struct List *) NFRMALLOC(sizeof(struct List*) * 1);
	ret_val = reverse(node);
	return ret_val;
}

