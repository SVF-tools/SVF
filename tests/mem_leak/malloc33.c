/*
 * Safe malloc and never free
 *
 * can not detect memory leak without heap cloning
 * we will issue a false negative FN
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from 177.mesa teximage.c line 1790

#include "aliascheck.h"

int** readcolor(){

	int**image = SAFEMALLOC(100);
	*image = LEAKFN(10); // FN
	return image;

}


int main(){

	int** image1 = readcolor();

	int** image2 = readcolor();

	free(*image1);
	free(image1);
	free(*image2);
	printf("%d%d",image1,image2);

}
