#set $dir=/tmp/nfs/mp
#set $filesize=1048576
#set $iosize=131072

set $dir=/tmp/bfs/mp
set $filesize=1039872
set $iosize=129984

set $count=8
set $nthreads=1
set $runlen=25

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
    flowop openfile name="openOP",filesetname="test-fileset",fd=1,directio,dsync
    flowop read name="seqread_multi_clientOP",fd=1,iters=$count,iosize=$iosize,directio,dsync
    flowop closefile name="closeOP",fd=1,directio,dsync

    # for writes
    #flowop createfile name="createOP",filesetname="test-fileset",fd=1,directio,dsync
    ##flowop writewholefile name="writewholefileOP",srcfd=1,fd=1,iosize=$iosize,directio,dsync
    #flowop write name="writeOP",fd=1,iosize=$iosize,iters=$count,directio,dsync
    #flowop closefile name="closeOP",fd=1,directio,dsync
    #flowop deletefile name="deleteOP",fd=1,directio,dsync
  }
}

run 5
