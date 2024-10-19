set $dir=MOUNTDIR
set $iosize=IOSZ
set $filesize=FILESZ
#set $filesize=cvar(type=cvar-gamma,parameters=mean:FILESZ;gamma:1.5,min=IOSZ,round=4096)
set $nthreads=NUMTHR
set $count=NUMIOS
set $numentries=1

set $runlen=60

#enable lathist

#writes
define fileset name="test-fileset",path=$dir,entries=$numentries,dirwidth=1,size=$filesize
#define fileset name="test-fileset",path=$dir,entries=$numentries,dirwidth=1,size=$filesize,prealloc,reuse

#reads
#define fileset name="test-fileset",path=$dir,entries=$numentries,dirwidth=1,size=$filesize,prealloc,reuse

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

    # for random/seq read
    #flowop openfile name="openOP",filesetname="test-fileset",fd=1,directio
    #flowop read name="readOP",fd=1,iters=$count,iosize=$iosize,directio
    #flowop closefile name="closeOP",fd=1,directio

    # for seq write (without prealloc)
    flowop createfile name="createOP",filesetname="test-fileset",fd=1
    flowop write name="writeOP",fd=1,iters=$count,iosize=$iosize,workingset=$filesize
    flowop fsync name="fsyncOP",fd=1
    flowop closefile name="closeOP",fd=1
    flowop deletefile name="deleteOP",fd=1
    
    # for random write (with prealloc)
    #flowop openfile name="openOP",filesetname="test-fileset",fd=1,directio,dsync
    #flowop write name="writeOP",fd=1,iters=$count,iosize=$iosize,workingset=$filesize,random,directio,dsync
    #flowop fsync name="fsyncOP",fd=1,directio,dsync
    #flowop closefile name="closeOP",fd=1,directio,dsync
  }
}

run $runlen
