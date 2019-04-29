## PinInstrument

To track data reuse, we need to keep a record of how many times a cache line is accessed before it is swapped out of cache. So we extend the reuse chain of a specific tag when there is a cache hit on that tag (no matter where the cache level is). We cut off the chain when a specific tag is swapped out of cache.

To track memory cost, we take a different approach. We take the memory hit and miss cost into consideration.