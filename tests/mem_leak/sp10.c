/*
 * Safe malloc for link list
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

// TC10: allocate and free structure
struct List {
	struct List * next;
	char * buf;
	int *a;
};

struct List * newNode() {
	struct List * node;
	node = (struct List *) SAFEMALLOC(sizeof(struct List));
	node->a = (int *) SAFEMALLOC(sizeof(int));
	return node;
}

int freeNode(struct List *node) {
	free(node);
	free(node->a);
}

void foo() {
	struct List *root;
	int a = 10;
	root = newNode();
	root->next = newNode();
	freeNode(root->next);
	freeNode(root);
}

void buffer_free(struct List **m) {
	free((*m)->buf);
	free(*m);
}

void buffer_init(struct List **x) {
	(*x)->buf = (char *) SAFEMALLOC(10);
}

void main() {
	struct List *m;
	m = (struct List *) SAFEMALLOC(sizeof(struct List));
	buffer_init(&m);
	buffer_free(&m);
}

