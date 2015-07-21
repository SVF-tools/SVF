/*
 * Safe malloc and Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */


#include "aliascheck.h"


void SerialReadBuf(){

	int * buf = NFRMALLOC(10);
	int n;
	while(n){

			printf("%d%d",n,buf);
			buf = PLKMALLOC(10);

		if(n){
			continue;
		}
		free(buf);
	}

	printf("%d",buf);

}

int main(){

	SerialReadBuf();
}
