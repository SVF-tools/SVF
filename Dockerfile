FROM ubuntu:18.04

# Stop script if any individual command fails.
RUN set -e

# Define LLVM version.
ENV llvm_version=10.0.0

# Define dependencies.
ENV lib_deps="make g++ git zlib1g-dev libncurses5-dev libssl-dev libpcre2-dev zip vim"
ENV build_deps="wget xz-utils cmake python git gdb"

# Fetch dependencies.
RUN apt-get update
RUN apt-get install -y $build_deps $lib_deps

# Fetch and extract SVF source.
RUN echo "Downloading LLVM and building SVF"
WORKDIR /
RUN wget "https://github.com/SVF-tools/SVF/archive/master.zip"
RUN unzip master.zip
WORKDIR /SVF-master
RUN bash ./build.sh
ENV PATH=/SVF-master/Release-build/bin:$PATH
RUN rm -rf /master.zip
