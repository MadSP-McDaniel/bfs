# Evaluation details

# Experimental Setup
Our experimental setup assumes a network composed of a Linux-based client (on a Raspberry Pi 4 machine running Debian) that mounts the file system, a Linux-based master server node (on an Intel NUC machine running Debian), and Linux-based storage nodes (on Raspberry Pi 4 machines running Debian). All hosts are connected by a local network over 1GbE interfaces:

<pre>
                           [ Client ]
                           192.168.1.2
                               |  
                           [ Master ]
                           192.168.1.1
                               |
       |---------------|-------|-------|---------------|
       |               |               |               |
  [ Storage ]     [ Storage ]     [ Storage ]     [ Storage ]
 192.168.1.205   192.168.1.206   192.168.1.207   192.168.1.208
</pre>

# Benchmark summary
Our benchmark suite consists of both microbenchmarks and macrobenchmarks exectuted at clients. For microbenchmarks, we study the isolated end-to-end performance for a client under standard workload profiles: sequential and random reads and writes. For macrobenchmarks, we study more complex workload profiles exihibited by Linux utilites and the scalability of the system across the number of clients and storage nodes. The microbenchmark scripts are located under the `micro/` directory and the macrobenchmark scripts under the `macro/` directory. The scripts assume that the mount points for file systems are located at `/tmp/bfs/mp`, `/tmp/bfs_ci/mp`, `/tmp/nfs/mp`, `/tmp/nfs_ci/mp`, etc. The file systems themsevles are assumed to be backed by a physical disk (i.e., not in-memory), and have sufficient usable capacity under the `/tmp` mount point.

# Software dependencies
- hyperfine (https://github.com/sharkdp/hyperfine)
- Python3.8+ (Matplotlib, NumPy, Pandas)

# Other software
- iotop
- iftop
- iperf/iperf3

# NFS details

Export a directory on the server for NFS:  

Export entry (without encryption): `/export/nfs/mp-unencrypted 127.0.0.1/32(rw,sync,subtree_check,no_root_squash)`
Export entry (with Kerberos encryption): `/export/nfs/mp-encrypted nfs-client.bfs.com(rw,sec=krb5p,all_squash,anonuid=1001,anongid=1002,subtree_check,sync)`

Export: `exportfs -a`

List exported directories: `showmount -e`

The export options are selected as follows:

- `rw`: read/write access
- `sync`: synchronous writes on server before responding to client requests
- `subtree_check`: force strong access checks. (Might check results with both options, both shouldn't matter under these workloads.)

  - Note: NFS really has two layers of access control: export access control and file-level access control. (1) The export access control checks that the accessed file is in the appropriate filesystem (which is easy) and also that it is in the exported _subtree_ (which is harder); i.e., that it is located _somewhere_ under the actual exported path on the server-local file system. So `subtree_check` here checks that the client is trying to access an exported file. (2) The file-level access control uses the uid and gid provided in each client NFS RPC request, where the same uids and gids are expected to be used on both the client and the server machine to maintain normal behavior. See `man 5 exports` for more details. Our method in BFS is equivalent to simply using `no_subtree_check` and exporting an entire NFS volume/directory (best practice), only focusing therefore on file-level access control. For simplicity, we can consider both the uids that BFS passes and the uids that NFSv3 (i.e., when using AUTH_SYS/no kerberos when mounting on a client) as authorization tokens, but we can easily substitute them later for valid kerberos credentials (see `man 5 nfs` for more details).
- `no_root_squash`: no effect (disable for easy cleaning)

Mount the server export on the client:
Mount (without encryption): `sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=131072,wsize=131072 192.168.1.1:/export/nfs/mp-unencrypted /tmp/nfs/mp`
Mount (with Kerberos encryption): `sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=131072,wsize=131072,sec=krb5p nfs-server.bfs.com:/export/nfs/mp-encrypted /tmp/nfs_ci/mp`

Or if added to fstab: `mount -a`

Unmount: `umount /tmp/bfs/mp`
Unmount: `umount /tmp/bfs_ci/mp`

The mount options are selected as follows:

- `sync`: flush file _data_ to server before returning to userspace (unsure if noac overrides)
- `noac`: a combination of the generic option `sync`, and the NFS-specific option `actimeo=0` (see `man 5 nfs` for more details on disabling caches vs file locking for better data/metadata coherence)
- `lookupcache=none`: disable dentry caching (so all getattrs on files are forced to go to server to retrieve the inode number). In combination with `noac`, after it goes to the server to get the unique file handle associated with the path (typically the inode number), it must also go to the server for GETATTR requests to get file attributes since no inode attributes will be cached either (see: <https://www.freesoft.org/CIE/Topics/115.htm> for more details for NFS opcodes).
- `rsize=4096`: payload size for NFS READ requests (read in 4k chunks as BFS does)
- `wsize=4096`: payload size for NFS WRITE requests (write in 4k chunks as BFS does)

We have a direct_io flag in the config to toggle it for bfs_client (i.e., bfs_client/FUSE does not cache any _data_ in the kernel). Be default the bfs_client code is also blocking on sockets for reading/writing file data, so it is the same as the NFS `sync` mount option. For metadata (file attributes), bfs_client also sets cache timeouts to 0 so we don't cache attributes either (fully synchronous).

(see `man 5 nfs` for more details)
Check `/etc/mtab` and `/proc/mounts` to diagnose errors.
Note: The NFS protocol is not designed to support true cluster file system cache coherence without some type of application serialization (see `man 5 nfs` for more details).

# BFS details

<!-- # NFS-Ganesha details -->

<!-- # Graphene-SGX details -->

<!-- # Ceph details -->

# Lighttpd setup notes
steps:
- Install lighthttpd and weighttp on nuc2
- Edit lighttpd config (sudo lighttpd -D -f /etc/lighttpd/lighttpd.conf) on nuc2 to change server.port (3001) and server.document-root (/var/www/html2)
- Make symlink from html2 to bfs mount point: sudo ln -s /tmp/bfs_ci/mp-c1/html /var/www/html2
- Start web server on nuc2: sudo lighttpd -D -f /etc/lighttpd/lighttpd.conf
- Start benchmark on nuc2: weighttp -n 1000 -c 10 -t 2 -k -H "User-Agent: foo" localhost:3001/index.html
    - Or just try to retrieve webpage: wget http://192.168.1.223:3001/index.html
- Disabled lighttp caching in config file

# Other Notes
Tools:
- Filebench
- IOZone
- Fio
- Specsfs (for NFS)

Macro applications:
- SQLite
- Lighttpd
- Redis
- RocksDB
- Various linux applications (e.g., building kernel with make, git, tar, grep, cp, etc.)

Common benchmarks:
- Yahoo! Cloud Serving Benchmark (YCSB)
- TPC-C
- TATP
- LevelDB benchmarks

Related systems:
- NFS/AFS/SMB
- Nexus (DSN’19)
- Obliviate(NDSS’18)
- Pesos(EuroSys’18)
- EnclaveDB(Oakland’18)
- Etc...
