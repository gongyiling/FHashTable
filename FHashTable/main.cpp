#include "fhash_table.h"
#include <map>
#include <stdlib.h>

int main()
{
	fhash_table<int, int> h;
	std::map<int, int> m;
	m.erase(1);
	for (int i = 0; i < 1000; i++)
	{
		auto r = rand();
		h.insert(r, i);
		const int* pi = h.find(r);
		assert(*pi == i);
		h.validate();
	}
	std::vector<int32_t> distance = h.get_distance_stats();
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
