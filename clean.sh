for i in `find . -name 'Debug-build'`
do
echo delete $i
rm -rf $i
done
for i in `find . -name 'Release-build'`
do
echo delete $i
rm -rf $i
done
