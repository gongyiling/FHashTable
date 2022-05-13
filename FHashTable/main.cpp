#include "fhash_table.h"

int main()
{
	fhash_table<int, int> h;
	h.insert(1, 1);
	const int* p = h.find(1);
	h.remove(1);
	return 0;
}
