TARGETDIR=`pwd`
#FLAGS='-daa -print-alias -debug-only=regpt'
#FLAGS='-daa '
FLAGS='-ins '
CLANGDEBUG= #-g
DVFHOME=/home/rocky/workspace/dvf/tools/Instrumentation
INSTRUMENTLIB=$DVFHOME/InstrumentLib.c
INSTRUMENTLIBHEADER=$DVFHOME/InstrumentLib.h

echo ++++entering $TARGETDIR for compilation
echo ++++compiling instrument lib $INSTRUMENTLIB
clang $CLANGDEBUG -c -emit-llvm $INSTRUMENTLIB -o $INSTRUMENTLIB.bc -I$INSTRUMENTLIBHEADER

for i in `find . -name '*.c'`
do
clang $CLANGDEBUG -c -emit-llvm $i -o $i.bc
echo ++++analyzing $i.bc
time dvf $1 $FLAGS $i.bc
llvm-dis $i.bc
llvm-dis $i.bc.dvf.bc

echo ++++linking $i.bc.dvf.bc and  $INSTRUMENTLIB.bc
llvm-link $i.bc.dvf.bc $INSTRUMENTLIB.bc > $i.linked.bc
llvm-dis $i.linked.bc

echo ++++generating executable file
#llc -O0 $i.linked.bc -o $i.linked.s
#clang++ $i.linked.s -o $i.linked -lm
llc -O0 -filetype=obj $i.linked.bc -o $i.linked.o    
clang $i.linked.o -o $i.linked -lm -lcrypt -lpthread

./$i.linked
done
echo ++++instrumentation finished

