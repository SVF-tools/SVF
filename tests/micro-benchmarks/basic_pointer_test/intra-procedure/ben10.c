int main(){

	int a,b,*p,i,c;

	if(i) 
		p =&a;
	else
		p = &b;

	*p = 10;

	c = *p;

	a = 5;

	b = 4;

	*p = 20;

	p = &a;

	*p = 10;

}
