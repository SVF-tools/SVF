int main(){
    int* a[100];
    int b,c,k;
    int e[20];

    e[0] = 100;

    k = e[0]; 
    a[15] = &b;

    a[18] = &c;
    
    *a[18] = 100;
    
    k = *a[18];

}
