for i in `find . -name '*.orig'`
do
echo analysing $i;
opt -mem2reg $i -o $i.opt
wpa -ander -dwarn -stat=true $i.opt
done
