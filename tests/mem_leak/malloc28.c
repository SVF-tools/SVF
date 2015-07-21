/*
 * Never free and safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from 181.mcf method implict.c

#include "aliascheck.h"

typedef struct network{

	int* arcs;
	int* stop_arcs;

}network;

struct network* net;

void func(struct network* net){

	int* arc = SAFEMALLOC(10);

	net->arcs = arc;	
	net->stop_arcs = NFRMALLOC(10);

}

int main(){

	struct network* net;
	net = SAFEMALLOC(20);
	func(net);

	free(net);
	free(net->arcs);

}
