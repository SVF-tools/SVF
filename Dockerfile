FROM ubuntu:16.04

# Stop script if any individual command fails.
RUN set -e

# Define LLVM version.
ENV llvm_version=7.0.0

# Define dependencies.
ENV lib_deps="make g++ git zlib1g-dev libncurses5-dev libssl-dev libpcre2-dev zip vim"
ENV build_deps="wget xz-utils cmake python"

# Fetch dependencies.
RUN apt-get update
RUN apt-get install -y $build_deps $lib_deps

# Fetch and extract LLVM source.
RUN echo "Building LLVM ${llvm_version}"
RUN mkdir -p /home/ysui/llvm-${llvm_version} 
WORKDIR /home/ysui/llvm-${llvm_version}
RUN wget "http://llvm.org/releases/${llvm_version}/llvm-${llvm_version}.src.tar.xz"
RUN tar xvf "llvm-${llvm_version}.src.tar.xz"
RUN wget "http://llvm.org/releases/${llvm_version}/cfe-${llvm_version}.src.tar.xz"
RUN tar xvf "cfe-${llvm_version}.src.tar.xz"
RUN mv "cfe-${llvm_version}.src" "llvm-${llvm_version}.src/tools/clang"
RUN rm "llvm-${llvm_version}.src.tar.xz"
RUN rm "cfe-${llvm_version}.src.tar.xz"
RUN mkdir llvm-${llvm_version}.obj
WORKDIR /home/ysui/llvm-${llvm_version}/llvm-${llvm_version}.obj
RUN cmake -DCMAKE_BUILD_TYPE=MinSizeRel ../llvm-${llvm_version}.src
RUN make -j4

# Fetch and extract SVF source.
RUN echo "Building SVF"
WORKDIR /
RUN wget "https://github.com/SVF-tools/SVF/archive/master.zip"
RUN unzip master.zip
WORKDIR /SVF-master
RUN bash ./build.sh


# Build and install LLVM from source.
#RUN mkdir build && cd build
#RUN cmake -j4 -DCMAKE_BUILD_TYPE=MinSizeRel ..
#RUN cmake -j4 --build .


# Cleanup LLVM source directory.
#rm -rf /src

# Cleanup build dependencies and apt cache.
RUN rm -rf /master.zip



