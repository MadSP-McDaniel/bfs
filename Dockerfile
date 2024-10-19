# The main BFS Dockerfile is useful for building a single container experimental/dev environment. Note that this only sets up the environment for running with the SGX libs in simulation mode.

FROM ubuntu:focal

# See sgx documentation for latest precompiled binary version
ENV DIST=ubuntu20.04
ENV BFS_HOME=/bfs
ARG DEBIAN_FRONTEND=noninteractive


## Install general dependencies
RUN /bin/bash -c "apt update && apt -y install linux-headers-$(uname -r) build-essential fuse3 libfuse3-3 libfuse3-dev libgcrypt-dev pkg-config ocaml ocamlbuild gdb zsh pip nano valgrind automake autoconf libtool wget python-is-python3 libssl-dev git cmake perl libcurl4-openssl-dev protobuf-compiler libprotobuf-dev libboost-all-dev protobuf-c-compiler libprotobuf-c-dev debhelper make reprepro unzip libgcrypt-dev iproute2 openssh-server nfs-kernel-server nfs-common kmod g++ apt-utils jq autotools-dev bison flex screen"


## Setup and install SGX SDK and PSW

# Clone the main linux-sgx repo for the SDK and PSW
RUN /bin/bash -c "git clone --recurse-submodules -j8 https://github.com/intel/linux-sgx.git /linux-sgx"
WORKDIR /linux-sgx

# Do pre-build tasks
RUN /bin/bash -c "./download_prebuilt.sh && make preparation"

# Copy over the mitigation tools (Intel binutils). Note that it appears they removed support for using the ld.gold linker
RUN /bin/bash -c "cp external/toolset/${DIST}/{as,ld,objdump} /usr/local/bin"

# Build the sdk installer with debug symbols. Should build the sdk first with debug symbols. Note that we don't need to build driver or PSW to run things in simulation mode. Just need the the main linux-sgx repo code and to build the SDK libs/binaries. The driver and PSW (i.e., urts lib and arch enclaves) will be simulated.
RUN /bin/bash -c "make sdk_install_pkg DEBUG=1"

# Install sdk
WORKDIR /linux-sgx/linux/installer/bin
ENV SGX_VERSION=2.18.101.1
RUN /bin/bash -c "./sgx_linux_x64_sdk_${SGX_VERSION}.bin --prefix /opt/intel"

# Setup environment variables (e.g., SGX_SDK path and LD_LIBRARY_PATH)
RUN /bin/bash -c "echo source /opt/intel/sgxsdk/environment >> /root/.bashrc"

# Still need to build the PSW libs so the enclave can actually use them from within the container (rather than using bind mounts for these on the host too). This should make debugging and such easier (note the driver kernel module should already be installed and running on the host system). Normally we would run the AEs and driver on the host though rather than from a (privileged) container.
WORKDIR /linux-sgx
RUN /bin/bash -c "source /opt/intel/sgxsdk/environment && make deb_psw_pkg DEBUG=1"
RUN /bin/bash -c "source /opt/intel/sgxsdk/environment && make deb_local_repo"
RUN /bin/bash -c "echo deb [trusted=yes arch=amd64] file:/linux-sgx/linux/installer/deb/local_repo_tool/../sgx_debian_local_repo focal main >> /etc/apt/sources.list"
RUN /bin/bash -c "apt update"
RUN /bin/bash -c "apt -y install libsgx-launch libsgx-urts libsgx-epid libsgx-quote-ex libsgx-dcap-ql"
# Note that current bfs benchmark scripts (client and server refresh) search for zshrc, so need to make sure when running things in containers we fix them to use bash or update container image to use zsh as default shell/root/.bashrc
RUN /bin/bash -c "echo export BFS_HOME=/bfs >> /root/.bashrc"

# Copy over sources for bfs
COPY . /bfs
WORKDIR /bfs

CMD ["/bin/bash"]
