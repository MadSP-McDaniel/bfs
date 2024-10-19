

TODO

    + move the bfs_device_list to bfs_device_list_t


Implementing the get/put blocks 

  [X] a) Modify the init function to use the configuration parameters
  [X] b) Create new read blocks and write blocks Functions (.h)
        + int readBlocks( bfs_vblock_list_t blks );
        + int writeBlocks( bfs_vblock_list_t blks );
  [X] c) Functions for the read and write blocks
        + seperate blocks into devices they are on
        + call read / write function
  [X] d) create the block error exception
  [X] e) implement the unit test

Implementing new allocation strategy

  [ ] a) add new configuration "allocation_discipline"
        + config values "linear" and "alternating"
        + create enum and strings for lookup (to layer)
        + add static variable holding allocation strategy
  [ ] b) add config check to bfsBlockLayer::bfsBlockLayerInit
  [ ] c) implement alternating disk 
        + if there are k disks
            + block is on disk vblk % k
            + bloc number is vlkb/k