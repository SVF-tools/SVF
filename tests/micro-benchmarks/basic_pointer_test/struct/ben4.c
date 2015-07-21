void foo(int*,int*);
typedef struct gg{
int f3;
}gg;

typedef struct ff{
int *f1;
gg f2;
}ff;
    
void ss(){
    
}

int main(){
    int *a,*b,obj,t,k;
    ff f,g;
    gg g1, g2;
    f.f2=g1;
    f.f1=&t;
    a=&obj;
    b=&t;
    foo(a,b);
    a = malloc(1);
    a = malloc(2);
    printf("%d",f.f2.f3);
}

void foo(int* x, int *y){
    *x = *y;
}
