/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

struct s_rr_node {
	short xlow; short xhigh; short ylow; short yhigh; 
	short ptc_num; short num_edges; 
	int *edges; 
	short *switches; float R; float C; 
};

struct s_rr_node *rr_node;

int* alloc(){

	return NFRMALLOC(10);
}

int main(){
	//rr_node = malloc(sizeof(struct s_rr_node)*10);
	int i;
	rr_node[i].edges = alloc();

	//free(rr_node[i].edges);

}
