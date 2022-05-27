# FHashTable
## Intruduction
FHashTable is an stl like hash table, it's highly cache friendly by allocating node together within the same bucket, most of the time the finding operation cost at most one cache miss.
## Details
Its basically is a large array. One part of node forms a Chained HashTable in the form of a doublely linked list, and the other part forms a binary tree. With the insertion and deletion of elements, the two parts of node can be converted to each other.
![image](https://github.com/gongyiling/FHashTable/blob/master/IMGS/fhash_table.png)
1. When initializing, all nodes in the array are formed into a binary tree in the order of subscripting.
2. When inserting element, let the binary tree assign us an node closest to the bucket and insert it.
## Performance
the benchmark environment: VS2019 + WIN10 + Intel Core i7 7700. The size of test key is 8 bytes, value is 16 bytes. TMap comes from UE4.24, TSherwoodHashTable is implemented based on https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp. TSherwoodMap data is incomplete, this is because when N exceeds 500K, TSherwoodMap memory explodes.

### FindSuccess
one hash table, find per element once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success.png" width="600" height="450" />


### FindSuccessCacheMissOne
large amount of hash table(larger than L3 cache size), find each hash table once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success_cache_missess_find_one.png" width="600" height="450" />


### FindSuccessCacheMissAll
large amount of hash table(larger than L3 cache size), find per element once.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/find_success_cache_misses_find_all.png" width="600" height="450" />


### Insert
one hash table.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/insert.png" width="600" height="450" />


### EffectMemory
the find operation may touched memory size in byte.

<img src="https://github.com/gongyiling/FHashTable/blob/master/IMGS/effect_memory.png" width="600" height="450" />
