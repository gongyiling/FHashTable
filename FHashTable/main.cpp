#include "fhash_table.h"
#include <map>
#include <stdlib.h>
#include <unordered_map>
#include <iostream>

int main()
{
	fhash_table<int, int> h;
	std::map<int, int> m;
	std::unordered_map<int, int> mm;
	for (int i = 0; i < 7000; i++)
	{
		auto r = rand();
		h.insert(r, i);
		h.validate();
		std::vector<int32_t> distances = h.get_distance_stats();
		int64_t sum = 0;
		for (size_t i = 0; i < distances.size(); i++)
		{
			sum += i * distances[i];
		}
		float avg = float(sum) / h.size();
		std::cout << avg << '\t' << h.load_factor() << std::endl;
	}

	h.reserve(2);
	h.validate();
	h.insert(1, 1);
	h.validate();
	if (true)
	{
		h.insert(2, 1);
		h.validate();
		h.insert(3, 1);
		h.validate();
		h.insert(4, 1);
		h.validate();
		h.insert(5, 1);
		h.validate();
	}
	const int* p = h.find(1);
	h.remove(1);
	h.validate();
	return 0;
}
