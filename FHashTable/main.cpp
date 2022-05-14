#include "fhash_table.h"
#include <map>
#include <stdlib.h>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <unordered_set>

std::vector<int32_t> gen_random_data(int32_t N)
{
	std::vector<int32_t> data;
	data.reserve(N);
	std::unordered_set<int32_t> numbers;
	for (int32_t i = 0; i < N; i++)
	{
		int32_t r = rand();
		if (numbers.insert(r).second)
		{
			data.push_back(r);
		}
	}
	return data;
}

void functional_test()
{
	{
		fhash_table<int32_t, int32_t> h;
		assert(h.find(0) == nullptr);
		assert(!h.remove(0));
		h.validate();
	}
	{
		fhash_table<int32_t, int32_t> h;
		h.insert(1, 1);
		h.validate();
		assert(h.find(0) == nullptr);
		assert(h.find(1) != nullptr);
		assert(!h.remove(0));
		assert(h.remove(1));
		h.validate();
	}
	{
		fhash_table<int32_t, int32_t> h;
		for (int32_t i = 0; i < 10; i++)
		{
			h.insert(i, i);
			assert(h.find(i) != nullptr);
			assert(h.remove(i));
			assert(h.find(i) == nullptr);
			h.validate();
		}
	}
	{
		fhash_table<int32_t, int32_t> h;
		std::vector<int32_t> data = gen_random_data(1000);
		for (size_t i = 0; i < data.size(); i++)
		{
			int32_t d = data[i];
			h.insert(d, i);
			assert(h.find(d) != nullptr);
			h.validate();
		}

		for (size_t i = 0; i < data.size(); i++)
		{
			int32_t d = data[i];
			const int32_t* pi = h.find(d);
			assert(*pi == i);
		}

		std::random_shuffle(data.begin(), data.end());
		for (size_t i = 0; i < data.size(); i++)
		{
			assert(h.remove(data[i]));
			h.validate();
		}
	}
}

int main()
{
	fhash_table<int, int> h;

	functional_test();

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
