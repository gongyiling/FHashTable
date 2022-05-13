#pragma once
#include <stdint.h>
#include <assert.h>
#include <memory>

template <typename key_t, typename value_t, int32_t entries_per_bucket = 2>
class fhash_table
{
public:

	static const int32_t invalid_index = -1;

	struct data
	{
		int32_t prev;
		int32_t next;
		key_t key;
		value_t value;
	};

	struct node
	{
		// node index start from -2 to -inf.
		// index + node_index + 2 = 0.
		static int32_t node_index_to_index(int32_t node_index)
		{
			return -2 - node_index;
		}

		static int32_t index_to_node_index(int32_t index)
		{
			return -2 - index;
		}

		int32_t& get_child_index(int32_t index)
		{
			int32_t* children = &lchild;
			return children[index];
		}

		int32_t lchild;
		int32_t rchild;
		int32_t parent;
	};

	union entry
	{
		data d;
		node n;
	};

	fhash_table() = default;

	fhash_table(fhash_table&& other)
	{
		m_entries = other.m_entries; 
		other.m_entries = nullptr;

		m_entries_size = other.m_entries_size;
		other.m_entries_size = 0;

		m_size = other.m_size; 
		other.m_size = 0;
	}

private:

	void rehash()
	{
		fhash_table old_table(std::move(*this));

		m_entries_size = std::max(m_size * entries_per_bucket, 4);
		m_entries = malloc(m_entries_size * sizeof(entry));

		// build the size tree.
		m_root = build_tree(0, m_entries_size);
		m_entries[m_entries_size].n.parent = invalid_index;
	}

private:
	node& get_node(int32_t node_index)
	{
		return m_entries[node::index_to_node_index(node_index)].n;
	}
	// size tree operation.
	int32_t build_tree(int32_t begin, int32_t end)
	{
		if (begin == end)
		{
			return invalid_index;
		}
		const int32_t mid = (begin + end) / 2;
		node& root = m_entries[mid].n;

		const int32_t lchild = build_tree(begin, mid);
		const int32_t rchild = build_tree(mid + 1, end);

		if (lchild != invalid_index)
		{
			get_node(lchild).parent = node::index_to_node_index(mid);
			root.lchild = node::index_to_node_index(lchild);
		}
		else
		{
			root.lchild = invalid_index;
		}

		if (rchild != invalid_index)
		{
			get_node(rchild).parent = node::index_to_node_index(mid);
			root.rchild = node::index_to_node_index(rchild);
		}
		else
		{
			root.rchild = invalid_index;
		}

		return mid;
	}

	int32_t get_node_dir(int32_t index)
	{
		node& n = m_entries[index].n;
		if (n.parent == invalid_index)
		{
			return invalid_index;
		}
		node& p = m_entries[n.parent].n;
		if (p.lchild == index)
		{
			return 0;
		}
		else if (p.rchild == index)
		{
			return 1;
		}
		else
		{
			assert(false);
			return invalid_index;
		}
	}

	template <int dir>
	int32_t step(int32_t index)
	{
		node& n = m_entries[index].n;
		int32_t pchild = invalid_index;
		int32_t child = n.get_child_index(dir);
		while (child != invalid_index)
		{
			pchild = child;
			child = m_entries[child].n.get_child_index(1 - dir);
		}
		return pchild;
	}

	void swap_node_index(int32_t a, int32_t b)
	{
		node& na = m_entries[a].n;
		node& nb = m_entries[b].n;
		const int32_t na_index = node::index_to_node_index(a);
		const int32_t nb_index = node::index_to_node_index(b);

		// swap parent.child index.
		const int32_t na_dir = get_node_dir(a);
		if (na_dir != invalid_index)
		{
			get_node(na.parent).get_child_index(na_dir) = nb_index;
		}
		const int32_t nb_dir = get_node_dir(b);
		if (nb_dir != invalid_index)
		{
			get_node(nb.parent).get_child_index(nb_dir) = na_index;
		}

		// swap child.parent index. 
		if (na.lchild != invalid_index)
		{
			get_node(na.lchild).parent = nb_index;
		}
		if (na.rchild != invalid_index)
		{
			get_node(na.rchild).parent = nb_index;
		}

		if (nb.lchild != invalid_index)
		{
			get_node(nb.lchild).parent = na_index;
		}
		if (nb.rchild != invalid_index)
		{
			get_node(nb.rchild).parent = na_index;
		}

		// swap node data
		std::swap(na.lchild, nb.lchild);
		std::swap(na.rchild, nb.rchild);
		std::swap(na.parent, nb.parent);
		// does not swap index, it's our data!
	}

	void remove_node(int32_t index)
	{
		node& n = m_entries[index].n;
		int32_t new_root = invalid_index;

		if (n.lchild != invalid_index && n.rchild != invalid_index)
		{
			int32_t swap_index = step<1>(index);
			assert(swap_index != invalid_index);
			swap_node_index(index, swap_index);
		}
		assert(n.lchild == invalid_index || n.rchild == invalid_index);

		if (n.lchild != invalid_index)
		{
			new_root = n.lchild;
		}
		else if (n.rchild != invalid_index)
		{
			new_root = n.rchild;
		}

		if (new_root != invalid_index)
		{
			const int32_t node_dir = get_node_dir(index);
			if (node_dir != invalid_index)
			{
				get_node(n.parent).get_child_index(node_dir) = new_root;
			}
			get_node(new_root).parent = n.parent;
		}
		else if (n.parent == invalid_index)
		{
			m_root = invalid_index;
		}
	}

	int32_t find(int32_t index, int32_t& last_dir) const
	{
		int32_t prev = invalid_index;
		int32_t current = m_root;
		while (current != invalid_index)
		{
			const node& n = m_entries[current].n;
			int32_t next = invalid_index;
			if (current == index)
			{
				return index;
			}
			if (n.lchild != invalid_index && index < node::node_index_to_index(n.lchild))
			{
				next = node::node_index_to_index(n.lchild);
				last_dir = 0;
			}
			else
			{
				next = node::node_index_to_index(n.rchild);
				last_dir = 1;
			}
			prev = current;
			current = next;
		}
		return prev;
	}

	void add_node(int32_t index)
	{
		int32_t last_dir;
		int32_t insert_index = find(index, last_dir);
		assert(insert_index < index);
		if (insert_index == invalid_index)
		{
			assert(m_root == invalid_index);
			m_root = index;
			node& n = m_entries[index].n;
			n.lchild = n.rchild = n.parent = invalid_index;
			return;
		}

		node& n = m_entries[index].n;
		n.lchild = n.rchild = invalid_index;

		int32_t& child_index = m_entries[insert_index].n.get_child_index(last_dir);
		assert(child_index == invalid_index);
		child_index = node::index_to_node_index(index);
		n.parent = node::index_to_node_index(insert_index);
	}

	int32_t bucket_size() const
	{
		return m_entries_size / entries_per_bucket;
	}

private:
	entry* m_entries = nullptr;
	int32_t m_entries_size = 0;
	int32_t m_size = 0;
	int32_t m_root = invalid_index;
};
