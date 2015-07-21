/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

typedef struct{
    int* f1;
	int* f2;
	int* f3;
}FOO;



void getfree(FOO* net){
 
	free(net->f1);
	free(net->f2);
	free(net->f3);
}

void readmin(){
	FOO net1;
	net1.f1 = (int*)NFRMALLOC(sizeof(int));
	net1.f2 = (int*)NFRMALLOC(2);
	net1.f3 = (int*)NFRMALLOC(3);
	printf("%d,%d,%d",net1.f1,net1.f2,net1.f3);

}

int main(){

	readmin();	
	//getfree(&net);
}

