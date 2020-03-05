FROM ubuntu:16.04

# Stop script if any individual command fails.
RUN set -e

# Define LLVM version.
ENV llvm_version=9.0.0

# Define dependencies.
ENV lib_deps="make g++ git zlib1g-dev libncurses5-dev libssl-dev libpcre2-dev zip vim"
ENV build_deps="wget xz-utils cmake python"

# Fetch dependencies.
RUN apt-get update
RUN apt-get install -y $build_deps $lib_deps

# Fetch and extract LLVM source.
RUN echo "Building LLVM ${llvm_version}"
RUN mkdir -p /home/jason/llvm
WORKDIR /home/jason/llvm
RUN wget "http://releases.llvm.org/9.0.0/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz"
RUN tar xf "clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz"
ENV LLVM_HOME=/home/jason/llvm/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin
ENV LLVM_DIR=$LLVM_HOME
ENV PATH=$LLVM_HOME:$PATH

# Fetch and extract SVF source.
RUN echo "Building SVF"
WORKDIR /
RUN wget "https://github.com/SVF-tools/SVF/archive/master.zip"
RUN unzip master.zip
WORKDIR /SVF-master
RUN bash ./build.sh
ENV PATH=/SVF-master/Release-build/bin:$PATH
RUN rm -rf /master.zip
