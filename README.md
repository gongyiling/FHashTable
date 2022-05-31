# FHashTable
## Intruduction
FHashTable is an STL-like hash table. It is highly cache-friendly due to allocating nodes together within the same bucket. Most of the time the cost of a finding operation is at most one cache miss.
## Details
It is basically a large array. One part of the nodes forms a chained hash table in the form of a doubly linked list, and the other part forms a binary tree. During the insertion and deletion of elements, two parts of nodes can be converted to each other.
![image](https://github.com/gongyiling/FHashTable/blob/master/IMGS/fhash_table.png)
1. When initializing, all nodes in the array form a binary tree in the order of access.
2. When inserting an element, let the binary tree assign us a node closest to the bucket and insert it.
## Performance
The benchmark environment: VS2019 + WIN10 + Intel Core i7 7700. The size of the test key is 8 bytes, value is 16 bytes. TMap comes from UE4.24, TSherwoodHashTable is implemented based on https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp. TSherwoodMap data is incomplete, since when N exceeds 500K, TSherwoodMap memory explodes.

### FindSuccess
One hash table, find each element once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success.png" width="600" height="450" />


### FindSuccessCacheMissOne
A large amount of hash tables (larger than L3 cache size), find each hash table once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success_cache_missess_find_one.png" width="600" height="450" />


### FindSuccessCacheMissAll
A large amount of hash tables (larger than L3 cache size), find each element once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success_cache_misses_find_all.png" width="600" height="450" />


### Insert
One hash table.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/insert.png" width="600" height="450" />


### EffectMemory
The find operation may touch memory size in bytes.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/effect_memory.png" width="600" height="450" />
