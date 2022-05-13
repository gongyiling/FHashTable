#pragma once
#include <stdint.h>

template <typename key_t, typename value_t>
class fhash_table
{
public:
	struct data
	{
		int32_t prev;
		int32_t next;
		key_t key;
		value_t value;
	};

	struct node
	{
		int32_t lchild;
		int32_t rchild;
		int32_t parent;
		int32_t size;
	};

};