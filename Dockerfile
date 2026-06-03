FROM ubuntu:24.04

# Stop ubuntu-20 interactive options.
ENV DEBIAN_FRONTEND noninteractive
ARG TARGETPLATFORM
# Stop script if any individual command fails.
RUN set -e

# Define LLVM version.
ENV llvm_version=21.1.0

# Define home directory
ENV HOME=/home/SVF-tools

# Define dependencies.
# Use the default python3-dev that ships with the Ubuntu base image (24.04
# ships python 3.12) instead of pulling python3.10-dev from a PPA — the
# Launchpad PPA infrastructure has been intermittently unreachable
# (HTTP 504 from add-apt-repository), and SVF itself does not pin a Python
# version, so the base-image python is sufficient.
ENV lib_deps="cmake g++ gcc git zlib1g-dev libncurses5-dev libtinfo6 build-essential libssl-dev libpcre2-dev zip libzstd-dev python3-dev"
ENV build_deps="wget xz-utils git tcl"

# Fetch dependencies.
RUN apt-get update --fix-missing
RUN apt-get install -y $build_deps $lib_deps

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
RUN ln -s ${Z3_DIR}/bin/libz3.so ${Z3_DIR}/bin/libz3.so.4
