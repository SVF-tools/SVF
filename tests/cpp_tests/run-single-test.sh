FLAGS="-g -S -emit-llvm"
FLAGS="${FLAGS} -I../"
#FLAGS="-g ${FLAGS}"

file=$1

#./clean.sh

clang++ ${FLAGS} -o ${file}.ll ${file}.cpp
opt -mem2reg -S -o ${file}.mem2reg.ll ${file}.ll
#clang++ -o ex ${file}.mem2reg.ll
wpa -ander -stat=false ${file}.mem2reg.ll

#find . -name "*.ll" -exec rm {} \;
find . -name "*.wpa" -exec rm {} \;
