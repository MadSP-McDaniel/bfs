set $dir=/tmp/bfs/mp
set $filesize=1m
set $iosize=4062
set $nthreads=1
set $runlen=10
define file name="testfile",path=$dir,size=$filesize,prealloc,reuse

# For open/close
set $nfiles=50
set $dw=5
define fileset name="test-fileset",path=$dir,entries=$nfiles,dirwidth=$dw,size=$filesize,prealloc,reuse

define process name="testerP",instances=1 {
  thread name="testerT",instances=1 {
    # test read
    flowop openfile name="openOP",filename="testfile",fd=1,directio,dsync
    flowop readwholefile name="readwholefileOP",fd=1,iosize=$iosize
    flowop closefile name="closeOP",fd=1

    # test write
    #flowop openfile name="openOP",filename="testfile",fd=1,directio,dsync
    #flowop writewholefile name="writewholefileOP",srcfd=1,fd=1,iosize=$iosize
    #flowop closefile name="closeOP",fd=1

    # test open/close
    #flowop openfile name="openOP",filesetname="test-fileset",fd=1
    #flowop closefile name="closeOP",fd=1
  }
}

run $runlen
