# Multithreaded opens/closes on a fileset.

set $dir=/mnt/nfs_we
set $nthreads=50
set $nfiles=100
set $dw=5
set $runlen=30

define fileset name="test-fileset",path=$dir,entries=$nfiles,dirwidth=$dw,prealloc,reuse

define process name="fileOC",instances=1 {
  thread name="fileOCthread",instances=$nthreads {
    flowop openfile name="openOP",filesetname="test-fileset",fd=1
    flowop closefile name="closeOP",fd=1
  }
}

run $runlen
