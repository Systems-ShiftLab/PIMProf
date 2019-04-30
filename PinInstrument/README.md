## PinInstrument

### Differences between data reuse cost and memory cost
The data reuse cost is the cost of the extra data transfer (data fetch / flush) that is introduced when the different regions of program executing on different places share the same data. 
The memory cost is the part of data fetch cost that has to be considered anyway with or without offloading.

The way to differentiate these two is that the memory access (either LOAD or STORE) that results in the data reuse cost is captured as a cache HIT by the shadow cache. Because only a HIT when running on one place can then become a MISS when running on different places, introducing an extra cost.
WThe memory access that results in the memory cost is already captured as a cache MISS by the shadow cache.


### Data reuse cost
To track data reuse, we need to keep a record of how many times a cache line is accessed before it is swapped out of cache. So we extend the reuse chain of a specific tag when there is a cache hit on that tag (no matter where the cache level is). We cut off the chain when a specific tag is swapped out of cache.

### Memory cost
To track memory cost, we take a different approach. We take the memory hit and miss cost into consideration.
