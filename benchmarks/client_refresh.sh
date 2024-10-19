#!/bin/bash
#
# Refreshes bfs client processes.
#

set -x
fs_type=$1
bench_type=$2
u=$3
iosz=$4
client_idx=$5
cip=$6
storage_type=$7
bkend=$8

if [ "${fs_type}" = "bfs" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        pkill -SIGKILL -f bfs_client
        sleep 5

        # wait for server to finish setting up
        _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    # after killing any old processes, try unmount old client with
    # the given index (cleans old FUSE mounts)
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    # then make sure mount point exists (create if necessary)
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    $BFS_HOME/build/bin/bfs_client -f -s -o allow_other /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    if [ "$client_idx" -gt 1 ]; then
        echo "Client idx > 1, skipping long sleep for mkfs"
        sleep 2
    else
        echo "Client idx == 1, doing long sleep for mkfs"
        if [ "$storage_type" = "remote" ]; then
            sleep 350
        else
            if [ "$bkend" = "lwext4" ]; then
                sleep 10
            else
                sleep 50
            fi
        fi
    fi

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
elif [ "${fs_type}" = "bfs_ci" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        pkill -SIGKILL -f bfs_client
        sleep 5

        # wait for server to finish setting up
        _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    # after killing any old processes, try unmount old client with
    # the given index (cleans old FUSE mounts)
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    # then make sure mount point exists (create if necessary)
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    $BFS_HOME/build/bin/bfs_client -f -s -o allow_other /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    if [ "$client_idx" -gt 1 ]; then
        echo "Client idx > 1, skipping long sleep for mkfs"
        sleep 2
    else
        echo "Client idx == 1, doing long sleep for mkfs"
        if [ "$storage_type" = "remote" ]; then
            sleep 350
        else
            if [ "$bkend" = "lwext4" ]; then
                sleep 10
            else
                sleep 50
            fi
        fi
    fi

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
elif [ "${fs_type}" = "nfs" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    # try unmount old client
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null
    sleep 10

    # make sure the mount point exists
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        sleep 5

        # wait for server to finish setting up
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=$iosz,wsize=$iosz \
        $_ip:/export/nfs/mp-unencrypted /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    # doing this on server now
    # if [ "$client_idx" -eq 1 ]; then
    #     # clear old files (just let c1 clear everything); note that BFS automatically clears everything when new server is started and client mounts
    #     sudo rm -rf /tmp/${fs_type}/mp-c$client_idx/* || true
    # fi

    sleep 10

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
elif [ "${fs_type}" = "nfsg" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    # try unmount old client
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null
    sleep 10

    # make sure the mount point exists
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        sleep 5

        # wait for server to finish setting up
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=$iosz,wsize=$iosz \
        $_ip:/export/nfsg/mp-unencrypted /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    # doing this on server now
    # if [ "$client_idx" -eq 1 ]; then
    #     # clear old files (just let c1 clear everything); note that BFS automatically clears everything when new server is started and client mounts
    #     sudo rm -rf /tmp/${fs_type}/mp-c$client_idx/* || true
    # fi

    sleep 10

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
elif [ "${fs_type}" = "nfs_ci" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    # try unmount old client
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null
    sleep 10

    # make sure the mount point exists
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        sleep 5

        # wait for server to finish setting up
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    # can use nfs-server.bfs.com or IP addr
    sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=$iosz,wsize=$iosz,sec=krb5p \
        $_ip:/export/nfs/mp-encrypted /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    # doing this on server now
    # if [ "$client_idx" -eq 1 ]; then
    #     # clear old files (just let c1 clear everything); note that BFS automatically clears everything when new server is started and client mounts
    #     sudo rm -rf /tmp/${fs_type}/mp-c$client_idx/* || true
    # fi

    sleep 10

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
elif [ "${fs_type}" = "nfsg_ci" ]; then
    echo -e "\nSetting up new ${fs_type} client ..."

    # try unmount old client
    sudo umount /tmp/${fs_type}/mp-c$client_idx &>/dev/null
    sleep 10

    # make sure the mount point exists
    mkdir -p /tmp/${fs_type}/mp-c$client_idx &>/dev/null

    _ip=$(grep "bfs_server_ip" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)

    if [ "$client_idx" -gt 1 ]; then
        echo "Multi-client, skipping server refresh"
    else
        # otherwise if its the first client, its new exp, so refresh the server
        # (single server so dont need to deal with refreshing multiple yet).

        # kill running client processes
        # pkill -SIGKILL -f filebench
        pkill -SIGKILL filebench # dont match entire command line (otherwise it will kill hypefine processes)
        sleep 5

        # wait for server to finish setting up
        # ssh bfs-exp "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type"
        ssh $u@$_ip "source ~/.zshrc ; \$BFS_HOME/benchmarks/server_refresh.sh ${fs_type} $bench_type $cip $storage_type $bkend"
        if [ "$?" -ne 0 ]; then
            echo "remote ssh command failed [$u@$_ip]"
            exit -1
        fi

        # wait for client to finish setting up (sleep for mkfs)
        rm -rf /tmp/filebench-shm-* &>/dev/null # clear filebench caches (only first client)
    fi

    # can use nfs-server.bfs.com or IP addr
    sudo mount -t nfs4 -o sync,noac,lookupcache=none,rsize=$iosz,wsize=$iosz,sec=krb5p \
        $_ip:/export/nfsg/mp-encrypted /tmp/${fs_type}/mp-c$client_idx \
        &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_client_current-c$client_idx.log &

    # doing this on server now
    # if [ "$client_idx" -eq 1 ]; then
    #     # clear old files (just let c1 clear everything); note that BFS automatically clears everything when new server is started and client mounts
    #     sudo rm -rf /tmp/${fs_type}/mp-c$client_idx/* || true
    # fi

    sleep 10

    # flush stuff
    # sync
    # echo 3 | sudo tee /proc/sys/vm/drop_caches
else
    echo "Unknown fs_type: ${fs_type}"
    exit -1
fi
