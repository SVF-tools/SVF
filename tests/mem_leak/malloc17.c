/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// this file simplified from 177.mesa image.c

#include "aliascheck.h"

int* gl_unpack_image3D(){

	int* image = SAFEMALLOC(10);
	return image;
}
int* gl_unpack_image(){

	return gl_unpack_image3D();
}


void glColorSubTableEXT(){

	int* image = gl_unpack_image();

	free(image);
	printf("%d\n",image);
}


int main(){


	glColorSubTableEXT();
}
