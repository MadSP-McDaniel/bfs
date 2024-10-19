#!/usr/bin/bash 
#
# bfs_dev_push.bash : push device implementation/config to clients

DEVICE_DIRECTORY=bfs_devices.txt
DEVPORT=2048
DEVBLOCKS=10000

# Check to see if the HOME variable is set
if [[ -z $BFS_HOME ]]; then
    echo "Environment variable BFS_HOME not set, aborting"
    exit -1 
fi

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

    # Setup the command 
    echo "Starting device ${DEVIPS[$i]}, disk id=${DEVDID[$i]}, ismaster=${DEVMST[$i]}"
    cmd="$BFS_HOME/build/bin/bfs_device -l $BFS_HOME/log/${DEVIPS[$i]}.log -d ${DEVDID[$i]} -p $DEVPORT -b $DEVBLOCKS 2> /dev/null > /dev/null &"
    if [[ "${DEVMST[$i]}" = "false" ]]; then
        cmd="ssh ${DEVIPS[$i]} '$cmd'"        
    fi

    # Run the command
    echo $cmd
    eval $cmd || exit -1

    # Move to the next device
    let i=$i+1
done
