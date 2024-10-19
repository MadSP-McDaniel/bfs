#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#set $dir=/tmp/bfs/mp
#set $dir=/tmp/nfs/mp
#set $nfiles=100
#set $meandirwidth=5
#set $filesize= 2031000 #2048000
#set $nthreads=1
#set $iosize=4062 #4096
#set $meanappendsize= 16248 #16384

define fileset name="bigfileset",path=$dir,entries=$nfiles,dirwidth=$meandirwidth,prealloc,size=$filesize,reuse

define process name="filereader",instances=$nthreads {
  thread name="filereaderthread",instances=$nthreads {
    #flowop createfile name="createOP1",filesetname="bigfileset",fd=1

    # Do sequential writes
    flowop openfile name="openOP1",filesetname="bigfileset",fd=1
    flowop writewholefile name="writeOP1",srcfd=1,fd=1,iosize=$iosize,directio
    flowop closefile name="closeOP1",fd=1

    # Append random writes
    flowop openfile name="openOP2",filesetname="bigfileset",fd=1
    flowop appendfilerand name="appendrandOP2",iosize=$meanappendsize,fd=1,directio
    flowop closefile name="closeOP2",fd=1

    # Do sequential reads
    flowop openfile name="openOP3",filesetname="bigfileset",fd=1
    flowop readwholefile name="readfileOP3",fd=1,iosize=$iosize
    flowop closefile name="closeOP3",fd=1
    
    # Delete + stat
    #flowop deletefile name=deletefile1,filesetname="bigfileset"
    #flowop statfile name=statfile1,filesetname="bigfileset"
  }
}

run RUNLEN
