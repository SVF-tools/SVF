FROM ubuntu:20.04

# Stop ubuntu-20 interactive options.
ENV DEBIAN_FRONTEND noninteractive

# Stop script if any individual command fails.
RUN set -e

# Define LLVM version.
ENV llvm_version=16.0.0

# Define home directory
ENV HOME=/home/SVF-tools

# Define dependencies.
ENV lib_deps="make g++-8 gcc-8 git zlib1g-dev libncurses5-dev libtinfo5 build-essential libssl-dev libpcre2-dev zip vim"
ENV build_deps="wget xz-utils git gdb tcl python"

# Fetch dependencies.
RUN apt-get update --fix-missing
RUN apt-get install -y $build_deps $lib_deps

# Install cmake and setup PATH
# Determine architecture and install CMake
RUN case "$TARGETPLATFORM" in \
    "linux/amd64") \
      arch="x86_64" ;; \
    "linux/arm64") \
      arch="aarch64" ;; \
    *) \
      echo "Unsupported platform: $TARGETPLATFORM" && exit 1 ;; \
  esac && \
  wget https://github.com/Kitware/CMake/releases/download/v$cmake_version/cmake-$cmake_version-linux-$arch.tar.gz -O cmake.tar.gz && \
  tar -xf cmake.tar.gz -C /home && rm -f cmake.tar.gz && \
  echo "export PATH=\"/home/cmake-${cmake_version}-linux-${arch}/bin:\$PATH\"" >> /etc/profile.d/cmake.sh

# Apply PATH modification
RUN mkdir -p ${HOME} && \ 
    echo "source /etc/profile.d/cmake.sh" >> ~/.profile

# Ensure that PATH is correctly set for interactive and non-interactive shells
SHELL ["/bin/sh", "-l", "-c"]

# Fetch and build SVF source.
RUN echo "Downloading LLVM and building SVF to " ${HOME}
WORKDIR ${HOME}
RUN git clone "https://github.com/SVF-tools/SVF.git"
WORKDIR ${HOME}/SVF
RUN echo "Building SVF ..."
RUN bash ./build.sh

# Export SVF, llvm, z3 paths
ENV PATH=${HOME}/SVF/Release-build/bin:$PATH
ENV PATH=${HOME}/SVF/llvm-$llvm_version.obj/bin:$PATH
ENV SVF_DIR=${HOME}/SVF
ENV LLVM_DIR=${HOME}/SVF/llvm-$llvm_version.obj
ENV Z3_DIR=${HOME}/SVF/z3.obj
