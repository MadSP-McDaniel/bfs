bfsDeviceLayer notes

Todo:

Local device implementation

New class hierarchy:

bfsDevice (pure virtual)
  bfsRemoteDevice
  bfsLocalDevice

[x] bfsNetworkDevice (device side implementation)
[x] bfsDeviceStorage (storage for blocks, mmap stuff)

** In devices directory
  [ ] Create the bfsBlockDevice pure virtual class
      - should include/unify the base functions
  [ ] Move remove/device to inherit from them
** (in bfsVertBlockCluster)
  [ ] Change bfs_remote_device_vec_t  to new class
      - rename the type (bfs_block_device_vec_t)
  [ ] Change get_devices function, definition of devices
  [ ] addBlockDevice - change definition
** in devices directory
  [x] Base class additions
      - path variable in the base class)
      - isRemote function in base class
  [x] change config
      - add storage_loc { local, remote }
      - add path to device (make this a variable in the base class)










  [X] a) Create the static class for bfsDeviceLayer
  [X] b) Move device log level, enums to device bfsDeviceLayer
  [X] c) Unit test for device layer
  [ ] d) Add signal handler to shut down (device), API (device layer)
          - propagate signal up through the layers
  [ ] e) thread each of the devices communication
  [ ] f) add shutdown function for device layer (call from utest)
  [ ] g) check all the destructors for leakage and cleanup
  [ ] h) add a configuration 
  [ ] j) creat UNKOWN defines for device IDs and user IDs
  [X] k) device layer unit test / each stored unit must be unique (avoid collisions)
  [X] l) create an initialization function for the bfsDeviceLayer
      + initialize the device layer log level
  [X] m) sort out the log levels
  [ ] n) add performance metrics / counts, summary on exit
  [ ] o) check for duplicate disk identifiers in init

  Getblocks/flexible buffer code
  [X] a) new structure for list of block devices (bfs_blocklist_t)
        common.h
        typedef vector<bfs_block_id> bfs_blockid_list_t;
        typedef map<bfs_block_idbfs_block_t * blk> bfs_block_list_t;
  [X] b) new message types (BFS_GET_BLOCKS, BFS_PUT_BLOCKS )
        + new message enum values (bfs_dev_common.h)
        + add strings to the string values (bfsDeviceLayer.cpp)
  [X] c) add code to remote device (bfsRemoteDevice)
        + putBlocks( block list )
        + getBlocks( idlist, block list )
  [X] e) device side -- add code to process new types (bfsDevice)
        + change get/put block to use flexible buffers (*.h, *.cpp)
        + move code to use flexible buffers - processCommunications
        + BFS_GET_BLOCKS - processClientRequest
        + BFS_PUT_BLOCKS - processClientRequest
  [X] f) add push blocks and get blocks to unit test (bfs_dev_utest.cpp)
        + Each side of the put/get block, modify to get 1...n blocks, where
          1 = get/put block and, 2+ is get/put blocks

  Flexible buffer code
  [X] a) new marshal code (bfsDeviceLayer.cpp/.h)
      + remove old marshal coding
  [X] g) move init, getblock, putblock to new marshal/buffer code
        + bfsRemoteDeviceInitialize (bfsRemnoteDevice.cpp)
        + getBlock (bfsRemnoteDevice.cpp)
        + putBlock (bfsRemnoteDevice.cpp)
  
  Crypto integration
  [X] a) Add setSecurityAssociation to bfsRemoveDevicebfsDevice, definition and code
        + add to headers (dev rem dev)
        + add to .cpp (dev and rem dev) - code
        + add to constructors and destructors
        + add crypto to makefiles
  [X] b) Add SA aquisition / config to getDeviceManifest
  [X] c) Add sa aquisition of SA bfsDeviceInitialize
  [X] d) Change mashal/unmarshal code
      + add security association to paramter list
      + check if NULL, abort if is
      + apply encryption/MAC as needed
  [X] e) Add SA configurations for the test devices

Code review:

    [x] bfsDeviceLayer.cpp / [x] bfsDeviceLayer.h
    [x] bfsDevice.cpp / [x] bfsDevice.h
    [x] bfsRemoteDevice.cpp / [x] bfsRemnoteDevice.h
    [x] bfs_device_main.cpp


Debugging notes:

  [x] - on initial connect, client sends probe request, no response (likely server side)
    + the server is hanging in the packetized recv waiting for data that will not come (
    + the server 
    )
  [x] - server closes/dies when client closes
      - the buffer being returned does not match the one sent, somehow the reteived function
      is somehow returning the wrong data.  Perhaps we try a simple  get/put unit test
      for the get/put block sections.
  [x] device stops when client discconnects

Topics for discussion

  - initialization / shutdown chain (static initializers vs call stack)
  - should every layer be a static class?


Notes for parser:

https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/03/Slides03.pdf
https://www.csd.uwo.ca/~mmorenom/CS447/Lectures/Syntax.html/node8.html