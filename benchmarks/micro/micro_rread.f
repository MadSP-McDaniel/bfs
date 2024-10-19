set $dir=MOUNTDIR
set $filesize=FILESZ
set $iosize=IOSZ
set $nthreads=NUMTHR
set $count=NUMIOS

# Description: random reads, fixed-size single file, preallocated, no reuse (i.e., no caching effects)
define fileset name="test-fileset",path=$dir,entries=1,dirwidth=1,size=$filesize,prealloc

define process name="filereader",instances=1 {
  thread name="filereaderthread",instances=$nthreads {
    flowop openfile name="openOP",filesetname="test-fileset",fd=1,directio,dsync
    flowop read name="rreadOP",fd=1,iters=$count,iosize=$iosize,directio,dsync,random
    flowop closefile name="closeOP",fd=1,directio,dsync
  }
}

run RUNLEN
