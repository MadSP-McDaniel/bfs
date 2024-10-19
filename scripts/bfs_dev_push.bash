#!/usr/bin/bash 
#
# bfs_dev_push.bash : push device implementation/config to clients

DEVICE_MANIFEST=bfs_device_manifest.txt
DEVICE_DIRECTORY=bfs_devices.txt

# Check to see if the HOME variable is set
if [[ -z $BFS_HOME ]]; then
    echo "Environment variable BFS_HOME not set, aborting"
    exit -1 
fi

# Make sure script is running in the scripts directory
cd $BFS_HOME/scripts

# Get the manifest data, check that the files are available
IFS='\n' command eval "manifest=($(cat $DEVICE_MANIFEST))"
for f in ${manifest[@]}; do
    if [[ ! ${f:${#f}-1:1} == '/' ]] && [[ ! -a $BFS_HOME/$f ]]; then
        echo "Unable to find manifest file : " $BFS_HOME/$f
        exit -1
    fi
done

# Get information about the manifest of devices
DEVIPS=()
DEVDID=()
DEVMST=()
while IFS=" " read -r IPADDR DID MASTER; do
    echo "Processing device $IPADDR, disk id=$DID, ismaster=$MASTER"
    DEVIPS+=($IPADDR)
    DEVDID+=($DID)
    DEVMST+=($MASTER)
done < $DEVICE_DIRECTORY

# Now walk all of the devices and perform the push
let i=0
while [[ $i < ${#DEVIPS[@]} ]]; do
    echo ${DEVIPS[$i]} " <-- " $i " ? " ${#DEVIPS[@]}
    echo "Processing device ${DEVIPS[$i]}, disk id=${DEVDID[$i]}, ismaster=${DEVMST[$i]}"

    # Now do the export of the file/directory
    if [[ "${DEVMST[$i]}" = "false" ]]; then
        for f in ${manifest[@]}; do
            if [[ ! ${f:0:1} == '#' ]]; then
                if [[ ${f:${#f}-1:1} == '/' ]]
                then
                    echo "Directory: " ${DEVIPS[$i]} "@" $f
                    cmd="ssh ${DEVIPS[$i]} 'mkdir -p $BFS_HOME/$f'"
                    echo $cmd
                    eval $cmd || exit -1
                else
                    echo "File: " ${DEVIPS[$i]} "@" $f 
                    cmd="scp $BFS_HOME/$f ${DEVIPS[$i]}:$BFS_HOME/$f"
                    echo $cmd
                    eval $cmd || exit -1
                fi
            fi
        done
    fi

    # Move to the next device
    let i=$i+1
done

