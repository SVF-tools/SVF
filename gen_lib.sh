# bash!
sysOS=`uname -s`
pwd
home=$pwd
echo $home
git clone https://github.com/SVF-tools/SVF-npm.git
if [[ $sysOS == "Darwin" ]]
then
  cp ./SVF/Release-build/lib/libSvf.a libSvf_mac.a
  cp ./SVF/Release-build/lib/CUDD/libCudd.a libCudd_mac.a
  cp ./libSvf_mac.a ./SVF-npm/Release-build/lib
  cp ./libCudd_mac.a ./SVF-npm/Release-build/lib
  cp -rf ./SVF/include ./SVF-npm/
  cd SVF-npm
  git status
  git add .
  git commit -m'update svflib'
  git status
elif [[ $sysOS == "Linux" ]]
then
  cp ./SVF/Release-build/lib/libSvf.a libSvf_ubuntu.a
  cp ./SVF/Release-build/lib/CUDD/libCudd.a libCudd_ubuntu.a
  cp ./libSvf_ubuntu.a ./SVF-npm/Release-build/lib
  cp ./libCudd_ubuntu.a ./SVF-npm/Release-build/lib
  cp -rf ./SVF/include ./SVF-npm/
  cd SVF-npm
  git status
  git add .
  git commit -m'update svflib'
  git status
fi
ls