int t;
main(){
	int **x, **y;
	int *a, *b, *c, *d,*e;
	x=&a; 
	*x = &e;
	for(t = 0;t< 10; t++)
	if(1) { x =&c;} 
	else { x= &d;} 
	*x = &t;
	
	
}

