#include "fhash_table.h"
#include <map>
#include <stdlib.h>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <chrono>

template <bool remove_duplicated>
std::vector<int32_t> gen_random_data(int32_t N)
{
	std::vector<int32_t> data;
	data.reserve(N);
	std::unordered_set<int32_t> numbers;
	for (int32_t i = 0; i < N; i++)
	{
		int32_t r = rand();
		if (remove_duplicated)
		{
			if (numbers.insert(r).second)
			{
				data.push_back(r);
			}
		}
		else
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
		std::vector<int32_t> data = gen_random_data<true>(1000);
		for (size_t i = 0; i < data.size(); i++)
		{
			int32_t d = data[i];
			h.insert(d, int32_t(i));
			assert(h.find(d) != nullptr);
			h.validate();
		}
		std::vector<int32_t> distances = h.get_distance_stats();
		int64_t sum = 0;
		for (size_t i = 0; i < distances.size(); i++)
		{
			sum += distances[i] * i;
		}
		int64_t sum10 = 0;
		for (size_t i = 0; i < 10; i++)
		{
			sum10 += distances[i];
		}
		float avg = float(sum) / h.size();
		double load_factor = h.load_factor();
		float factor10 = float(sum10) / h.size();
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
			assert(h.find(data[i]) == nullptr);
			h.validate();
		}
	}
}

static _declspec(noinline) void test_std_unordered_map()
{
	for (int32_t i = 1; i < 15; i++)
	{
		const int32_t N = std::pow(3, i);
		std::cout << "N = " << N << std::endl;
		std::vector<int32_t> data = gen_random_data<false>(N);
		{
			std::unordered_map<int64_t, int64_t> m;
			for (int32_t i : data)
			{
				m.emplace(i, i);
			}

			int64_t sum = 0;
			auto start = std::chrono::high_resolution_clock::now();
			for (int32_t i = 0; i < 100000000 / N; i++)
			{
				for (int32_t i : data)
				{
					sum += m.find(i)->second;
				}
			}
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			std::cout << "std::unordered_map, elapsed milliseconds: " << elapsed << " sum: " << sum << std::endl;
		}
	}
}

static _declspec(noinline) void test_fhash_table()
{
	for (int32_t i = 1; i < 15; i++)
	{
		const int32_t N = std::pow(3, i);
		std::cout << "N = " << N << std::endl;
		std::vector<int32_t> data = gen_random_data<false>(N);
		{
			fhash_table<int64_t, int64_t> m;
			for (int32_t i : data)
			{
				m.insert(i, i);
			}

			auto start = std::chrono::high_resolution_clock::now();
			int64_t sum = 0;
			for (int32_t i = 0; i < 100000000 / N; i++)
			{
				for (int32_t i : data)
				{
					sum += *m.find(i);
				}
			}
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			std::cout << "std::fhash_table, elapsed milliseconds: " << elapsed << " sum: " << sum << " load_factor: " << m.load_factor() << std::endl;
		}
	}
}

static void perf_test()
{
	test_std_unordered_map();
	test_fhash_table();
}

int main()
{
	fhash_table<std::string, int> hh;
	hh.insert("ddd", 1);
	fhash_table<std::string, int> hhh = hh;
	functional_test();
	perf_test();
	return 0;
}
