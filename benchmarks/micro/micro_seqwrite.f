set $dir=MOUNTDIR
set $filesize=FILESZ
set $iosize=IOSZ
set $nthreads=NUMTHR
set $count=NUMIOS

# Description: sequential writes, fixed-size single file, no preallocation, no reuse (i.e., no caching effects)
define fileset name="test-fileset",path=$dir,entries=1,dirwidth=1,size=$filesize

define process name="filewriter",instances=1 {
  thread name="filewriterthread",instances=$nthreads {
    flowop createfile name="createOP",filesetname="test-fileset",fd=1,directio,dsync
    flowop write name="seqwriteOP",fd=1,iters=$count,iosize=$iosize,directio,dsync
    flowop closefile name="closeOP",fd=1,directio,dsync
    flowop deletefile name="deleteOP",fd=1,directio,dsync
  }
}

run RUNLEN
