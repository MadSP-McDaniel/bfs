#!/bin/bash
#
# Refreshes bfs server processes.
#

set -e -x
fs_type=$1
bench_type=$2
cip=$3
storage_type=$4
bkend=$5 # unused for now

# if running in bfs mode refresh bfs server and client processes before next workload (gets rid of any caching effects). For NFS we will need to refresh at some point.
# might need to sleep before setting up new processes to let server/client log any measurements and let mkfs go through
# use -SIGTERM for now since SIGINT having issues gracefully shutting down bg processes
if [ "${fs_type}" = "bfs" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # kill running server processes
    pkill -SIGKILL -f bfs_server || true
    sleep 5

    # for bfs, then set up the bfs config with correct device types
    if [ "$storage_type" = "remote" ]; then
        sed -i.tmp -e 's/local/remote/g' $BFS_HOME/config/bfs_system_config.cfg >$BFS_HOME/config/bfs_system_config.cfg.tmp
        # then set up any remote devices (using same user as client specified for server login)
        num_devices=$(grep "d._type" $BFS_HOME/config/bfs_system_config.cfg | wc -l)
        for did in $(seq 1 $num_devices); do
            dtype=$(grep "d${did}_type" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
            if [ "$dtype" = "remote" ]; then
                dip=$(grep d${did}_ip $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
                ssh $USER@$dip "source ~/.zshrc ; \$BFS_HOME/benchmarks/device_refresh.sh ${fs_type} $bench_type $did"
                if [ "$?" -ne 0 ]; then
                    echo "remote ssh command failed [$USER@$dip]"
                    exit -1
                fi
            fi
        done
    else
        sed -i.tmp -e 's/remote/local/g' $BFS_HOME/config/bfs_system_config.cfg >$BFS_HOME/config/bfs_system_config.cfg.tmp
    fi

    # wait for server/TEE to finish setting up (sleep for TEE load)
    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear old filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs
    $BFS_HOME/build/bin/bfs_server &>$BFS_HOME/benchmarks/o/${fs_type}_server_current.log &
    sleep 10

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
elif [ "${fs_type}" = "bfs_ci" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # kill running server processes (match bfs_server* since bfs_ci might run after bfs)
    pkill -SIGKILL -f bfs_server || true
    sleep 5

    # for bfs, then set up the bfs config with correct device types
    if [ "$storage_type" = "remote" ]; then
        sed -i.tmp -e 's/local/remote/g' $BFS_HOME/config/bfs_system_config.cfg >$BFS_HOME/config/bfs_system_config.cfg.tmp
        # then set up any remote devices (using same user as client specified for server login)
        num_devices=$(grep "d._type" $BFS_HOME/config/bfs_system_config.cfg | wc -l)
        for did in $(seq 1 $num_devices); do
            dtype=$(grep "d${did}_type" $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
            if [ "$dtype" = "remote" ]; then
                dip=$(grep d${did}_ip $BFS_HOME/config/bfs_system_config.cfg | head -1 | cut -d ":" -f2 | xargs)
                ssh $USER@$dip "source ~/.zshrc ; \$BFS_HOME/benchmarks/device_refresh.sh ${fs_type} $bench_type $did"
                if [ "$?" -ne 0 ]; then
                    echo "remote ssh command failed [$USER@$dip]"
                    exit -1
                fi
            fi
        done
    else
        sed -i.tmp -e 's/remote/local/g' $BFS_HOME/config/bfs_system_config.cfg >$BFS_HOME/config/bfs_system_config.cfg.tmp
    fi

    # wait for server/TEE to finish setting up (sleep for TEE load)
    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear old filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs
    $BFS_HOME/build/bin/bfs_server_ne &>$BFS_HOME/benchmarks/o/${fs_type}_server_current.log &
    sleep 10

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
elif [ "${fs_type}" = "nfs" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear any filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs

    # check that server is exporting to the specified client
    exportOK=$(cat /etc/exports | grep $cip)
    if [ "$exportOK" == "" ]; then
        echo "Export not correct at server"
        exit -1
    fi

    # delete old files for new experiment
    sudo rm -rf /export/nfs/mp-unencrypted/* || true

    check_serv=$(ps aux | grep --line-buffered ganesha)
    echo $check_serv | grep --line-buffered ganesha | grep ganesha.nfsd &>/dev/null
    if [ "$?" -eq 0 ]; then
        echo "Incorrect nfs server type running, killing it"
        # exit -1
        sudo systemctl stop nfs-ganesha
    fi

    # To be safe, we only continue if nfs-ganesha server is not running (as checked above); once we reach here there are no other nfs-ganesha related services to kill
    sudo systemctl restart nfs-kernel-server
    sleep 100 # might not need 100 for kernel NFS server

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
elif [ "${fs_type}" = "nfs_ci" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear any filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs

    # check that server is exporting to the specified client
    exportOK=$(cat /etc/exports | grep $cip)
    if [ "$exportOK" == "" ]; then
        echo "Export not correct at server"
        exit -1
    fi

    # delete old files for new experiment
    sudo rm -rf /export/nfs/mp-encrypted/* || true

    check_serv=$(ps aux | grep --line-buffered ganesha)
    echo $check_serv | grep --line-buffered ganesha | grep ganesha.nfsd &>/dev/null
    if [ "$?" -eq 0 ]; then
        echo "Incorrect nfs server type running, killing it"
        # exit -1
        sudo systemctl stop nfs-ganesha
    fi

    # To be safe, we only continue if nfs-ganesha server is not running (as checked above); once we reach here there are no other nfs-ganesha related services to kill
    sudo systemctl restart nfs-kernel-server
    sleep 100 # might not need 100 for kernel NFS server

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
elif [ "${fs_type}" = "nfsg" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear any filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs

    # check that server is exporting to the specified client
    exportOK=$(cat /etc/ganesha/ganesha.conf | grep $cip)
    if [ "$exportOK" == "" ]; then
        echo "Export not correct at server"
        exit -1
    fi

    # delete old files for new experiment
    sudo rm -rf /export/nfsg/mp-unencrypted/* || true

    # Since we have to do some extra setup steps when dealing with nfs servers, here we check if incorrect nfs server is running (just fail stop for now so we can check that results were not corrupted from using wrong server)
    # The following check works as follows: we grep the line buffered output as a workaround to ignore the grep process that is listed in the process snapshot; first we grep for the kernel-server/ganesha process, then grep for the actual server process, then grep again to check whether such a process exists or not (indicating the server is live or not)
    check_serv=$(ps aux | grep --line-buffered "\[nfsd\]")
    echo $check_serv | grep --line-buffered "\[nfsd\]" &>/dev/null
    if [ "$?" -eq 0 ]; then
        echo "Incorrect nfs server type running, killing it"
        # exit -1
        sudo systemctl stop nfs-kernel-server
        sudo systemctl stop rpc-gssd.service
    fi

    # restart nfs-ganesha daemon to setup clean env (really segfaults after heavy use...) then wait for it to exit GRACE mode (~90s)
    # TODO: need to check if it's OK to have some unused services running or if we need to kill those too, like rpc-gssd.service, rpc-svcgssd.service, and nfs-idmapd.service (when running both nfs and nfs-ganesha benchmarks); we still need krb5-admin-server.service and krb5-kdc.service though
    # For debugging: sudo systemctl status krb5-admin-server.service krb5-kdc.service nfs-kernel-server rpc-svcgssd.service rpc-gssd.service nfs-idmapd.service nfs-ganesha
    # sudo systemctl stop nfs-kernel-server # dont need, since we are assuming that it should never exist above to be safe; we still may need to kill other services that may be lurking around (notably rpc-gssd.service)
    sudo systemctl stop rpc-gssd.service rpc-svcgssd.service nfs-idmapd.service
    sleep 5
    sudo systemctl restart nfs-ganesha
    sleep 100

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
elif [ "${fs_type}" = "nfsg_ci" ]; then
    echo -e "\nSetting up new ${fs_type} server ..."

    # rm -rf /tmp/filebench-shm-* &>/dev/null # clear any filebench caches (edit: dont need on server)
    mkdir -p $BFS_HOME/benchmarks/o &>/dev/null # for server logs

    # check that server is exporting to the specified client
    exportOK=$(cat /etc/ganesha/ganesha.conf | grep $cip)
    if [ "$exportOK" == "" ]; then
        echo "Export not correct at server"
        exit -1
    fi

    # delete old files for new experiment
    sudo rm -rf /export/nfsg/mp-encrypted/* || true

    # Since we have to do some extra setup steps when dealing with nfs servers, here we check if incorrect nfs server is running (just fail stop for now so we can check that results were not corrupted from using wrong server)
    # The following check works as follows: we grep the line buffered output as a workaround to ignore the grep process that is listed in the process snapshot; first we grep for the kernel-server/ganesha process, then grep for the actual server process, then grep again to check whether such a process exists or not (indicating the server is live or not)
    check_serv=$(ps aux | grep --line-buffered "\[nfsd\]")
    echo $check_serv | grep --line-buffered "\[nfsd\]" &>/dev/null
    if [ "$?" -eq 0 ]; then
        echo "Incorrect nfs server type running, killing it"
        # exit -1
        sudo systemctl stop nfs-kernel-server
        sudo systemctl stop rpc-gssd.service
    fi

    # restart nfs-ganesha daemon to setup clean env (really segfaults after heavy use...) then wait for it to exit GRACE mode (~90s)
    # sudo systemctl stop nfs-kernel-server # dont need, since we are assuming that it should never exist above to be safe
    sudo systemctl stop rpc-gssd.service rpc-svcgssd.service nfs-idmapd.service
    sleep 5
    sudo systemctl restart nfs-ganesha
    sleep 100

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5

    wait
else
    echo "Unknown fs_type: ${fs_type}"
    exit -1
fi
