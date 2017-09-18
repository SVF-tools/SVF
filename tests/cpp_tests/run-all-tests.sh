FLAGS="-S -emit-llvm"
FLAGS="${FLAGS} -I../"
#FLAGS="-g ${FLAGS}"

FILES=`ls *.cpp`

for file in ${FILES}
do
	file=${file%.*}
	echo ${file}
	clang++ ${FLAGS} -o ${file}.ll ${file}.cpp
	opt -mem2reg -S -o ${file}.mem2reg.ll ${file}.ll
	wpa -ander -stat=false ${file}.mem2reg.ll

	find . -name "*.ll" -exec rm {} \;
	find . -name "*.wpa" -exec rm {} \;
done
