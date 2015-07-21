/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from 177.mesa teximage.c line 1790

#include "aliascheck.h"


int* readcolor(){

	int*image = NFRMALLOC(100);

	return image;

}


int main(){

	int* image1 = readcolor();


	printf("%d",image1);

}
