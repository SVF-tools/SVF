int main(){
    int *p, *s,*r,*w,*q,*x;
    int ***m,**n,*z,*y,z1,y1;

    m=&n;
    n=&z;
    *m=&y;
    z=&z1;
    y=&y1;
    ***m=10;
    z=**m;
}
