int a,b;
struct foo{
	int x;
	int y;

};

typedef struct foo FOO;

void* alloc(int n){
	return malloc(n);

}

void allocation(int** s){

	*s = malloc(10);
}
FOO *ex(){

	int *q;
	FOO *p;
	allocation(&p);
	/*
	FOO *w;
	p = (FOO*)alloc(sizeof(FOO));
	p->x = a;
	p->y = b;
	q = &a;
	*q = b;
	*/
	return p;

}

void main(){

	ex();

}
