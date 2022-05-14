#include "fhash_table.h"
#include <map>

int main()
{
	fhash_table<int, int> h;
	std::map<int, int> m;
	m.erase(1);
	for (int i = 0; i < 1000; i++)
	{
		h.insert(i, i);
		const int* pi = h.find(i);
		assert(*pi == i);
		h.validate();
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
