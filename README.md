# `Securing Cloud File Systems with Trusted Execution`

BFS is a distributed filesystem that enables securely storing and retrieving files across a cluster of untrusted server and storage hosts. It leverages Intel SGX to bootstrap secure protocols, providing four key properties: 1) hardware-enforced confidentiality and integrity protection for all file system data and metadata, 2) protection against semantic attacks at the interface between the SGX enclave and the host it runs on (e.g., well-known Iago attacks), 3) secure and high-performance file sharing, 4) flexible policy management.

BFS is a research prototype. See the full paper at: https://doi.org/10.1109/TDSC.2024.3474423.
Reference:
```
@ARTICLE{bbh+24,
    TITLE = {{Securing Cloud File Systems with Trusted Execution}},
    AUTHOR = {Quinn Burke and Yohan Beugin and Blaine Hoak and Rachel King and Eric Pauley and Ryan Sheatsley and Mingli Yu and Ting He and Thomas La Porta and Patrick McDaniel},
    JOURNAL = {IEEE Transactions on Dependable and Secure Computing (TDSC)},
    MONTH = {September},
    YEAR = {2024}
}
```

## Project structure

```
├── benchmarks/         # benchmark workload/plotting scripts
├── build/              # makefiles to build each bfs subsystem and unit tests
├── config/             # config files for enclave (.edl), nfs, kerberos, and bfs in general (.pem)
├── doc/                # generated doc files
├── docker/             # scripts for docker compose env setup
├── scripts/            # misc debugging scripts
└── src/                # source code for all bfs subsystems
     ├── bfs_blk/       # block management
     ├── bfs_client/    # clients
     ├── bfs_comms/     # network comms
     ├── bfs_device/    # storage devices
     ├── bfs_fs/        # core file system
     └── bfs_utils/     # shared utility functions
```

## Environment setup

### Bare metal

> This assumes an Ubuntu 20.04 system. On a fresh OS install, first run: `sudo apt update`. Then:
>
> <details><summary>BFS Server</summary>
>
> - SGX kernel driver:
>   - Install kernel headers: `sudo apt-get install linux-headers-$(uname -r)`.
>   - Clone the out-of-tree kernel driver (isgx) repo: <https://github.com/intel/linux-sgx-driver>.
>   - Build the kernel module: `cd linux-sgx-driver ; make`.
>   - Install the driver: ```sudo mkdir -p "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx" && sudo cp isgx.ko "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx" && sudo sh -c "cat /etc/modules | grep -Fxq isgx || echo isgx >> /etc/modules" && sudo /sbin/depmod && sudo /sbin/modprobe isgx```.
>   - Note: Don't need to build driver or PSW to run things in simulation mode. Just need the the main linux-sgx repo code and to build the SDK libs/binaries. The driver and PSW (i.e., urts lib and arch enclaves) will be simulated.
>
> - SGX PSW and SDK:
>   - Install dependencies: `sudo apt-get install build-essential ocaml ocamlbuild automake autoconf libtool wget python-is-python3 libssl-dev git cmake perl libssl-dev libcurl4-openssl-dev protobuf-compiler libprotobuf-dev debhelper cmake reprepro unzip libgcrypt-dev gdb valgrind libboost-all-dev protobuf-c-compiler libprotobuf-c-dev`.
>   - Clone the repo: `git clone https://github.com/intel/linux-sgx.git`.
>   - Do pre-build tasks: `cd linux-sgx && make preparation` (may have to call `./download_prebuilt.sh` if make does not execute it).
>   - Copy over the mitigation tools (Intel binutils): `sudo cp external/toolset/{current_distr}/{as,ld,objdump} /usr/local/bin`.
>   - Build the sdk: `make sdk DEBUG=1`.
>   - Build the sdk installer: `make sdk_install_pkg DEBUG=1`.
>   - Install sdk to a specified path: `cd linux/installer/bin ; ./sgx_linux_x64_sdk_${version}.bin`.
>   - Set up sdk environment variables: `source <SDK_PATH>/environment` (and add it to shell rc file). Note that this needs to be done before building platform software; see <https://github.com/intel/linux-sgx/issues/466>. Then build/run the sample code in simulation mode to test it (for hardware mode need to install the psw first).
>   - Build the psw: `make psw DEBUG=1` (builds psw libraries/architectural enclaves).
>   - Build the psw installer: `make deb_psw_pkg DEBUG=1`.
>   - Build a local repository for the psw: `make deb_local_repo` (installed to linux/installer/deb/sgx_debian_local_repo). To add the local repository to the system repository configuration, append the following line to /etc/apt/sources.list: `deb [trusted=yes arch=amd64] file:/<PATH_TO_LOCAL_REPO> focal main`. Then run an update: `sudo apt update`.
>   - Install the psw using the local package repository: `sudo apt install libsgx-launch libsgx-urts libsgx-epid libsgx-quote-ex libsgx-dcap-ql`.
>   - Start/stop aesmd service: `service aesmd start`
>
> </details>
> <details><summary>BFS Client</summary>
>
> - Install fuse library and dev packages: `sudo apt install fuse3 libfuse3-3 libfuse3-dev jq autotools-dev autoconf bison flex gdb valgrind screen`.
>
> </details>
> <details><summary>BFS Device</summary>
>
> - No dependencies

### Docker

> We have two options for using Docker to set up an experimental/dev environment. One builds a single Docker image that will contain the SGX libs and BFS code (assuming the aesmd service and sgx kernel driver are running on the host), and the other uses the pre-packaged SGX Dockerfile and compose file to build a multi-container setup (running the aesmd service in a container).
>
> <details><summary>Simple image build</summary>
> To build a container image containing the bfs {server, client, device} binaries from the given Dockerfile:
>
> - From the root of the project repo: `docker build --platform linux/amd64 -t bfs-base .` (Note: building the docker image takes ~24 mins, and running the container to build the new binaries takes ~48s)
>
> To create a new container from the image, need to enable access to the driver device file and the aesmd socket as follows:
>
> - `docker run --device=/dev/isgx -v /var/run/aesmd:/var/run/aesmd -it bfs-base`
>
> - Note: Docker builder defaults to using sh so we invoke bash to handle certain string expansion features to make building sdk easier (see discussion at <https://github.com/moby/moby/issues/7281>).
>
> - Note: When running the client in a container it requires access to the FUSE device file, so we need to run the container as privileged for now (see discussion at <https://github.com/moby/moby/issues/16233>).
> - Note: [TODO] Currently trying to run the SGX-mode binaries even in simulation mode does not seem to work on ARM (MAc M1/M2 chips), so we can just debug with the debug/non-sgx binaries (e.g., `bfs_server`)
>
> </details>
>
> <details><summary>Docker compose build</summary>
> [TBD]
> </details>

### Debian package

> We also have a gitlab ci configuration set up to package some of the BFS binaries for quickly getting them running. Assuming the system has the necessary dependencies like FUSE, SGX, etc. installed, this will allow skipping the BFS build.
>
> <details><summary>Installing the debian package</summary>
>
> - Download the deb package from the gitlab ci pipeline artifacts, then run: `sudo dpkg -i bfs-dist.deb` (Note: the binaries will be installed into `/opt/bfs`, and the server depends on loading the system config which expects to have an environment variable `BFS_HOME` defined)
> - To uninstall: `sudo dpkg -r bfs-dist`
>
> </details>

## Usage

Below, we highlight the four ways to use the BFS repo: 1) running a simple BFS server and client, 2) running the various unit tests, 3) running the benchmark suite against a BFS server/client, or 4) running the benchmark suite against a different file system server/client. There are only two steps to do beforehand: editing the configuration/benchmark files and then building the binaries.

>  <details><summary>Prestart tasks</summary>
>
> First edit the configuration files on servers and clients:
>
>   1. First make sure the environment variable `BFS_HOME` is set on both the server and client. All programs are executed from, and all benchmark data is output to, a path under it.
>   2. Then set configuration parameters as desired in `enclave_config.xml` and `bfs_system_config.cfg` on the server and client. Note that the default values in `enclave_config.xml` should generally be fine to use, while a few things in `bfs_system_config.cfg` should probably be changed (`bfs_server_ip`, `bfs_server_port`, the `path`/`size` for each device, and if using remote devices the associatied `ip`/`port`).
>   3. If running the provided benchmarks, next configure the benchmark settings in `run_bench.sh` on the client. Note that the default settings should generally be fine to use, but `cip` should be changed appropriately to the client's ip addr. Note that for some of the macrobenchmarks (e.g., lighttpd/weighttp), the client's FUSE mount may be accessed by a user other than the user who mounted the file system. It therefore requires enabling `user_allow_other` in `/etc/fuse.conf` (see https://man7.org/linux/man-pages/man8/mount.fuse3.8.html) and mounting with `-o allow_other`.
>
> Then build BFS:  
>
>   4. To build all subsystems: `make all SGX_MODE={HW,SIM} BUILD_LVL={0,1,2}`
>   5. If running benchmarks, filebench requires to disable ASLR: `echo 0 | sudo tee /proc/sys/kernel/randomize_va_space`
>
> </details>

<br>

> <details><summary>Running a simple BFS server and client</summary>
>
> First start the server (Note: kill any server processes still running):
>
> 1. Non-SGX/debug mode: `./bfs_server`
> 2. SGX mode: `./bfs_server_ne`
>
> Then start the client (Note: unmount any previously mounted BFS instances):
>
> 3. Run: `bfs_client -f -s -o allow_other <test mount point>`
>
> Some example workloads:
>
> - `ls <test mount point>`
> - `printf 'f%.0s' {1..1000} >> <test mount point>/test.txt`
> - `filebench -f $BFS_HOME/benchmarks/micro/micro_test2.f` (Note: make sure to edit the workload parameters appropriately, particularly `dir, filesize, and iosize`, and ensure they reflect the correct file system block size 4096/4062)
>
> </details>

<br>

> <details><summary>Running tests</summary>
>
> We have a variety of unit tests that can be used to debug different BFS subsystems:
>
> - Block layer: `./bfs_blk_utest [TODO]`
> - Client layer: `./bfs_client_test [TODO]`
> - Comms layer: `./bfs_comm_utest -v -a "127.0.0.1" -p 9992` (remove addr field for server)
> - File system layer: `./bfs_core_test -c`
> - Storage device layer: `./bfs_dev_utest [TODO]`
> - Utility functions: `./bfs_util_utest -v -<test flag> and ./bfs_util_utest_ne -v -<test flag>`
>
> </details>

<br>

> <details><summary>Running the benchmark suite</summary>
>
> Make sure environment is clean, then simply run: `./run_bench.sh &>bench.log`. This will run all of the benchmarks specified in the bench script, save all benchmark results to the data output directories (`benchmarks/micro/output` and `benchmarks/macro/output`), and save all benchmark output messages to a log file.
> </details>

<br>

> <details><summary>Running other distributed file systems</summary>
>
> In our evaluation, we compare the performance of BFS against some widely-used distributed file systems. We use the same benchmark suite that was run against a BFS server/client. Below, we provide details on the environment setup for these systems used in our evaluation (see `benchmarks/README.md` for more specific details on the server/client export/mount options):
>
> - NFS:
>   - Disable the nfs-ganesha server/systemd service: `sudo systemctl stop nfs-ganesha`
>   - Start the normal nfs kernel server: `sudo systemctl start nfs-kernel-server`
>   - Export a directory by editing `/etc/exports`, allowing the appropriate authentication types (`sec=sys` or `sec=krb5p`), then running `exportfs -a` to do the export. Note: a copy of this config file is stored here in `config/exports`)
>   - Mount the directory on the client: `sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=<any>,wsize=<any>,sec={sys,krb5p} <server ip>:<nfs export> <mount point>`
>   - Check that export was OK: `sudo showmount -e`
> - NFS-Ganesha:
>   - Disable the normal nfs kernel server/systemd service: `sudo systemctl stop nfs-kernel-server`
>   - Start the nfs-ganesha server: `sudo systemctl start nfs-ganesha`
>   - Export a directory by editing `/etc/ganesha/ganesha.conf`, allowing the appropriate authentication types (`Sectype=sys,krb5p;` at least), then the nfs-ganesha server should automatically read and update the exports. Note: a copy of this config file is stored here in `config/ganesha.conf`)
>   - Mount the directory on the client: `sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=<any>,wsize=<any>,sec={sys,krb5p} <server ip>:<nfs-ganesha export> <mount point>`
>   - Check that export was OK: `sudo showmount -e`
> - NFS+Graphene-SGX port: [TODO]
>   - [TODO]
> - Ceph: [TODO]
>   - [TODO]
>
> </details>

<br>


## Acknowledgements
This work was supported in part by the Semiconductor Research Corporation (SRC) and DARPA. This work was also supported in part by the National Science Foundation under award CNS-1946022. The views and conclusions contained in this document are those of the authors and should not be interpreted as representing the official policies, either expressed or implied, of the Combat Capabilities Development Command Army Research Laboratory or the U.S. Government. The U.S. Government is authorized to reproduce and distribute reprints for Government purposes not withstanding any copyright notation here on.
