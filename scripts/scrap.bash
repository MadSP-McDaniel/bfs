# Now walk all of the devices in the bfs
export IFS='\n' command eval "clients=($(awk 'IFS="\n" {print $0}' $DEVICE_DIRECTORY))"
IFS='\n' read -r -a clients < $DEVICE_DIRECTORY
echo $clients
for c in ${clients[@]}; do

    # Parse out the device values
    IFS=', ' read -r -a array <<< "$c"
    IPADDR=${array[0]}
    DID=${array[1]}
    MASTER=${array[2]}
    echo "Processing device " $IPADDR ", disk id=" $DID, ", ismaster=" $MASTER

        echo $MASTER
    # Now do the export of the file/directory
    if [[ "$MASTER" = "false" ]]; then
        echo $MASTER
        for f in ${manifest[@]}; do
            if [[ ! ${f:0:1} == '#' ]]; then
                if [[ ${f:${#f}-1:1} == '/' ]]
                then
                    echo "Directory: " $IPADDR "@" $f
                    cmd="ssh $IPADDR 'mkdir -p $BFS_HOME/$f'"
                    echo $cmd
                    eval $cmd || exit -1
                else
                    echo "File: " $IPADDR "@" $f 
                    cmd="scp $BFS_HOME/$f $IPADDR:$BFS_HOME/$f"
                    echo $cmd
                    eval $cmd || exit -1
                fi
            fi
        done
    fi

done


#IFS='\n' command eval "clients=($(awk 'IFS='\n' {print $1}' bfs_clients.txt))"
#IFS='\n' command eval "clientids=($(awk '{print $2}' bfs_clients.txt))"
#IFS='\n' command eval "ismaster=($(awk '{print $3}' bfs_clients.txt))"


# Now do the export of the file/directory
for c in ${clients[@]}; do 
    for f in ${manifest[@]}; do
        if [[ ! ${f:0:1} == '#' ]]; then
            if [[ ${f:${#f}-1:1} == '/' ]]
            then
                echo "Directory: " $c "@" $f
                cmd="ssh $c 'mkdir -p $BFS_HOME/$f'"
                echo $cmd
                eval $cmd || exit -1
            else
                echo "File: " $c "@" $f 
                cmd="scp $BFS_HOME/$f $c:$BFS_HOME/$f"
                echo $cmd
                eval $cmd || exit -1
            fi
        fi
    done
done