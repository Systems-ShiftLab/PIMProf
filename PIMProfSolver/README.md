## PIMProfSolver

### Differences between data reuse cost and memory cost
The data reuse cost is the cost of the extra data transfer (data fetch / flush) that is introduced when the different regions of program executing on different places share the same data. 
The memory cost is the part of data fetch cost that has to be considered anyway with or without offloading.

The way to differentiate these two is that the memory access (either LOAD or STORE) that results in the data reuse cost is captured as a cache HIT by the shadow cache. Because only a HIT when running on one place can then become a MISS when running on different places, introducing an extra cost.
WThe memory access that results in the memory cost is already captured as a cache MISS by the shadow cache.


### Data reuse cost
To track data reuse, we need to keep a record of how many times a cache line is accessed before it is swapped out of cache. So we extend the reuse chain of a specific tag when there is a cache hit on that tag (no matter where the cache level is). We cut off the chain when a specific tag is swapped out of cache.

### Memory cost
To track memory cost, we take a different approach. We take the memory hit and miss cost into consideration.


### reference output
BBL	CPUIns		PIMIns		CPUMem		PIMMem		difference
1	1797		17970		713264		688974		8117
2	6		60		4152		4087		11
3	5		50		712		582		85
4	848		8480		137524		132239		-2347
5	137		1370		5136		4746		-843
6	5		50		4144		4079		20
7	5		50		4112		4047		20
8	5		50		4112		4047		20
9	3		30		108		108		-27
10	0		0		0		0		0
11	5		50		20164		20034		85
12	2		20		248		248		-18
13	1		10		36		36		-9
14	4		40		276		211		29
