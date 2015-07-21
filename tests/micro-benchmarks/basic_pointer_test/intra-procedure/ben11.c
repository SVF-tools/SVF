int main(){



	int *p, *q, c,d;

	p  =&c;
	q = &d;

	p = q;

	*p = 100;
	*q = 100;
}
