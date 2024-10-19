
README.txt : Using the device scripts

These are notes for using the scripts, which are a work in progress as of
6/24/21.   PDM will update these as we continue to make progress on the 
system.

Assumptions: 

    - the devices have been setup per the install instructions (should be fine for current)
    - the device have BFS_HOME set, and it at ~/bfs

Instructions:

    For now, you will be logged into the master device with a terminal window and run command
    for various device control functions.  There are two files that have configuration 
    information in them:

    bfs_devices.txt - this has the list of the devices in the system with additional config
    information about each.  The current config has IP, device ID, and a bool (master=y/n).

    bfs_device_manifest.txt - this file contains the list of files and directories that need
    to be copied/created on the non-master devices.

    Commands:

    bfs_dev_push.bash - this command takes the current local executables and builds the 
    directories, configs, and everything else on the remove devices.

    bfs_dev_start.bash - this command starts up the device code on the remove devices as well
    as the local (master device).

    bfs_dev_stop.bash - this command starts up the device code on the remove devicesas well
    as the local (master device).

Notes for quinn burke:

    - we dont' need the bfsDevice to be compiled in SGX, as it is the device implementation.
    This will _never_ run in the SGX unless we have enclaves on the devices.

TODO:

    + modify the device code to get the port and IP from the system cponfiguration,
    rather than receiving them from the command line.  This modification should 
    reorganize the ≈ code as follows:

        - bfsDevice::bfsDeviceInitialize: move the system config get our particular device 
        config, 313-330-ish
        - bfsDevice::bfsDeviceInitialize: move the initialization of the mmap and comms below
        the get in the previous setp
        - bfsDevice::bfsDevice: remove block and port number from constructor
        - bfs_device_main.cpp:main: remove the -p and -b from the command line

    + complete the json-rpc implementation of remote commands, which are implemented in the
    bfsdev_command.py and bfsdev_crtl.py files.

    + Add a configuration item to the path for the mmap.

    + Add a health check script to check that all of the devices are running, which will check
    that the device is up and running (use pgrep bfs_device and make sure it is non-zero)


