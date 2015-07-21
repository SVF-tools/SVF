void foo(int **s){
    
}
int main(){

    int* a;
    int* b;
    int**c;
    
    int x,y;

    int *w;
    int **s;
    int u;
    a=&x;
    b=&y;
    c=&a;
    s = c;
    s=&w;
    w=&u;
    u = 9;
    foo(s); 
    if(x==5){
    	**c = 10;
    }
    else{
	*b = 100;
    }

    while(u>10){
	*w = 100;
    }
    return 0;

}
