# Multithreaded opens/closes on a fileset.

set $dir=MOUNTDIR
set $nthreads=NUMTHR
set $nfiles=500
set $dw=50

define fileset name="test-fileset",path=$dir,entries=$nfiles,dirwidth=$dw,prealloc,reuse

define process name="fileOC",instances=1 {
  thread name="fileOCthread",instances=$nthreads {
    flowop openfile name="openOP",filesetname="test-fileset",fd=1
    flowop closefile name="closeOP",fd=1
  }
}

run RUNLEN
