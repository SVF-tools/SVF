for i in `find . -name '*.bc'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.ll'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.dot'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.opt'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.out'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.s'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.o'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.linked'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name 'out'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name 'out1'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name 'out2'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.saber'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.wpa'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.dda'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.instr'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.mta'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.dvf'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.simd'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.exe'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.funptr'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.su'`
do
echo delete $i
rm -rf $i
done

for i in `find . -name '*.tex'`
do
echo delete $i
rm -rf $i
done

CLIENT="funptr\
		all\
		cast\
		free\
		uninit"
for client in $CLIENT;
do
		for i in `find . -name "*.$client.*"`;
		do
			echo delete $i;
			rm $i;
		done
done

rm -rf compile.log
