# Buffer-Manager
buffer manager
The buffer manager manages a fixed number of pages in memory that represent pages from a page file managed by the storage manager(https://github.com/dganesh07/Storage-Manager). 
The memory pages managed by the buffer manager are called page frames or frames for short. We call the combination of a page file and the page frames storing pages from that file a Buffer Pool. The Buffer manager is able to handle more than one open buffer pool at the same time. However, there can only be one buffer pool for each page file. Each buffer pool uses one page replacement strategy (LRU,clock etc)that is determined when the buffer pool is initialized. 
