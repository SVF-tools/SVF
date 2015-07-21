struct agg{
    int a;
    struct fgg{
	int k;
    }fgg;
}agg;

int main(){
    struct agg str1;
    struct agg* str2 = &str1;

    str2->fgg.k = 100;    

}
