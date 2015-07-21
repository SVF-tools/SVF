/*
 * Safe malloc and never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from spec2000 181.mcf

#include "aliascheck.h"

typedef struct network
{
	long feas_tol;
	long pert_val;
	long bigM;
	double optcost;  
	int *arcs;
	int *stop;
	int *nodes;
} network_t;

int bar(int* s){
	free(s);
}

int foo(network_t* net){
	int *p = SAFEMALLOC(10);
	int *q = NFRMALLOC(10);
	net->arcs = p;
	net->stop = q;
	bar(net->arcs);
	//bar(net->stop);
}

int main(){


	network_t net;
	foo(&net);

}
