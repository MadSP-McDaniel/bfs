set $filesize=1073741824
set $iosize=131072
set $dir=/mnt/bfs_ci

set $count=8
set $nthreads=1
set $runlen=60

#enable lathist

#writes
#define fileset name="test-fileset",path=$dir,entries=1,dirwidth=1,size=$filesize

#reads
define fileset name="test-fileset",path=$dir,entries=1,dirwidth=1,size=$filesize,prealloc,reuse

define process name="filereader",instances=1 {
  thread name="filereaderthread",instances=$nthreads {
    #flowop createfile name="createOP",filename="testfile",fd=1,directio,dsync
    #flowop openfile name="openOP",filename="testfile",fd=1,directio,dsync
    #flowop readwholefile name="readwholefileOP",fd=1,iosize=$iosize,directio,dsync
    #flowop read name="readOP",fd=1,iters=$count,iosize=$iosize,random,directio,dsync
    #flowop readwholefile name="readwholefileOP",fd=1,iosize=$iosize,directio,dsync
    #flowop read name="readOP",fd=1,iters=$count,iosize=$iosize,directio,dsync
    #flowop write name="writeOP",fd=1,iters=$count,iosize=$iosize,directio,dsync
    #flowop closefile name="closeOP",fd=1,directio,dsync

    # for reads
    flowop openfile name="openOP",filesetname="test-fileset",fd=1
    flowop read name="seqreadOP",fd=1,iters=$count,iosize=$iosize,random
    flowop closefile name="closeOP",fd=1

    # for write (without prealloc)
    #flowop createfile name="createOP",filesetname="test-fileset",fd=1,directio,dsync
    #flowop write name="writeOP",fd=1,iosize=$iosize,iters=$count,directio,dsync
    #flowop closefile name="closeOP",fd=1,directio,dsync
    #flowop deletefile name="deleteOP",fd=1,directio,dsync
    
    # for write (with prealloc)
    #flowop openfile name="openOP",filesetname="test-fileset",fd=1,directio,dsync
    #flowop write name="rwriteOP",fd=1,iters=$count,iosize=$iosize,directio,dsync,random
    #flowop closefile name="closeOP",fd=1,directio,dsync
  }
}

run $runlen
