#  !bash
# type './build.sh'  for release build
# type './build.sh debug'  for debug build

#########
# VARs and Links
########
ROOT_PATH=$(pwd)
sysOS=`uname -s`
MacLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-apple-darwin.tar.xz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
MacZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-osx-10.14.6.zip"
UbuntuZ3="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
LLVMHome="llvm-10.0.0.obj"
Z3Home="z3.obj"
SVFTests="Test-Suite"

########
# Download LLVM binary and Z3 binary
########
if [[ $sysOS == "Darwin" ]]
then
       if [ ! -d "$LLVMHome" ]
       then
       		echo 'Downloading LLVM binary for MacOS '
      		curl -L $MacLLVM > llvm-mac.tar.xz
      	 	mkdir ./$LLVMHome && tar -xf "llvm-mac.tar.xz" -C ./$LLVMHome --strip-components 1
		rm llvm-mac.tar.xz
       fi
       if [ ! -d "$Z3Home" ]
       then
       		echo 'Downloading Z3 binary for MacOS '
      		curl -L $MacZ3 -o z3_mac.zip
      		unzip -q "z3_mac.zip" && mv ./z3-* ./$Z3Home
		rm z3_mac.zip
       fi
elif [[ $sysOS == "Linux" ]]
then
       if [ ! -d "$LLVMHome" ]
       then
       		echo 'Downloading LLVM binary for Ubuntu'
      		wget -c $UbuntuLLVM -O llvm-ubuntu.tar.xz
      		mkdir ./$LLVMHome && tar -xf "llvm-ubuntu.tar.xz" -C ./$LLVMHome --strip-components 1
		rm llvm-ubuntu.tar.xz
       fi
       if [ ! -d "$Z3Home" ]
       then
       		echo 'Downloading LLVM binary for Ubuntu'
      		wget -c $UbuntuZ3 -O z3_ubuntu.zip
      		unzip -q "z3_ubuntu.zip" && mv ./z3-* ./$Z3Home
		rm z3_ubuntu.zip
       fi
else
	echo 'not support builds in OS other than Ubuntu and Mac'
fi


export LLVM_DIR=$ROOT_PATH/$LLVMHome
export Z3_DIR=$ROOT_PATH/$Z3Home
export PATH=$LLVM_DIR/bin:$PATH
echo "LLVM_DIR =" $LLVM_DIR

########
# Download SVF Test Suite
#######

if [ ! -d "$SVFTests" ]
then
   git clone "https://github.com/SVF-tools/Test-Suite.git"
   if [[ $sysOS == "Linux" ]] ; then
   	cd ./$SVFTests
   	./generate_bc.sh
   	cd ..
   fi
fi


########
# Build SVF
########
if [[ $1 == 'debug' ]]
then
	rm -rf ./'Debug-build'
	mkdir ./'Debug-build'
	cd ./'Debug-build'
	cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
	rm -rf ./'Release-build'
	mkdir ./'Release-build'
	cd ./'Release-build'
	cmake ../
fi
make -j4

#########
# Run ctest
########
if [[ $sysOS == "Linux" ]] ; then
  ctest
fi

########
# Set up environment variables of SVF
########
cd ../
if [[ $1 == 'debug' ]]
then
  . ./setup.sh debug
else
  . ./setup.sh
fi
#########
# Optionally, you can also specify a CXX_COMPILER and your $LLVM_HOME for your build
# cmake -DCMAKE_CXX_COMPILER=$LLVM_DIR/bin/clang++ -DLLVM_DIR=$LLVM_DIR
#########


