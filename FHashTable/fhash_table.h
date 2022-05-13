#pragma once
#include <stdint.h>
#include <assert.h>
#include <memory>

template <typename key_t, typename value_t, typename hasher_t = std::hash<key_t>, int32_t entries_per_bucket = 2>
class fhash_table
{
public:
	template <typename tag>
	struct integer_t
	{
		int32_t value;
		integer_t() = default;
		constexpr explicit integer_t(int32_t v) :value(v) {}
		integer_t operator = (integer_t other){ value = other.value; return *this; }

		bool operator < (integer_t other) const{return value < other.value;}
		bool operator <= (integer_t other) const { return value <= other.value; }
		bool operator == (integer_t other) const { return value == other.value; }
		bool operator != (integer_t other) const { return value != other.value; }
		bool operator >= (integer_t other) const { return value >= other.value; }
		bool operator > (integer_t other) const { return value > other.value; }

		friend integer_t operator + (integer_t a, integer_t b) { return integer_t(a.value + b.value); }
		friend integer_t operator / (integer_t a, integer_t b) { return integer_t(a.value / b.value); }
	};

	struct tag_index {};
	struct tag_node_index {};

	using index_t = integer_t<tag_index>;
	using node_index_t = integer_t<tag_node_index>;

	static constexpr index_t invalid_index = index_t(-1);

	static constexpr node_index_t invalid_node_index = node_index_t(-2);

	// node index start from -3 to -inf.
	// index + node_index + 3 = 0.
	static index_t node_index_to_index(node_index_t node_index)
	{
		return index_t(-3 - node_index.value);
	}

	static node_index_t index_to_node_index(index_t index)
	{
		return node_index_t(-3 - index.value);
	}

	struct data
	{
		index_t prev;
		index_t next;
		key_t key;
		value_t value;
	};

	struct node
	{
		node_index_t& get_child_index(index_t dir)
		{
			node_index_t* children = &lchild;
			return children[dir.value];
		}

		node_index_t lchild;
		node_index_t rchild;
		node_index_t parent;
	};

	union entry
	{
		data d;
		node n;
		bool is_data() const
		{
			return d.prev >= invalid_index;
		}
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

	~fhash_table()
	{
		clear();
	}

	void clear()
	{
		if (m_entries != nullptr)
		{
			for (int32_t i = 0; i < m_entries_size; i++)
			{
				entry& e = m_entries[i];
				if (e.is_data())
				{
					e.d.~data();
				}
			}
			free(m_entries);
			m_entries = nullptr;
		}
		m_entries_size = 0;
		m_size = 0;
		m_root = invalid_index;
	}

private:

	index_t compute_hash(key_t key) const
	{
		return index_t(m_hasher(key) % bucket_size());
	}

	void insert_empty(data& d, key_t key, value_t value)
	{
		d.key = key;
		d.value = value;
		d.next = invalid_index;
		d.prev = invalid_index;
	}

	index_t allocate_entry(index_t index)
	{
		index_t last_dir;
		index_t pos = find(index, last_dir);
		assert(pos != invalid_index);
		// remove from tree.
		remove_node(pos);
		return pos;
	}

	index_t insert_tail(index_t index, key_t key, value_t value)
	{
		index_t new_index = allocate_entry(index);
		index_t prev = index;
		while (index != invalid_index)
		{
			prev = index;
			index = get_entry(index).d.next;
		}
		data& p = get_entry(prev).d;
		data& t = get_entry(new_index).d;
		p.next = new_index;
		t.prev = prev;
		t.next = invalid_index;
		return new_index;
	}

	index_t insert(key_t key, value_t value)
	{
		index_t index = compute_hash(key);
		return insert_index(index, key, value);
	}

	index_t insert_index(index_t index, key_t key, value_t value)
	{
		try_grow();
		entry& e = get_entry(index);
		data& d = e.d;
		if (e.is_data())
		{
			if (d.prev != invalid_index)
			{
				// we are list from other slot.
				key_t victim_key = e.key;
				value_t victim_value = e.value;

				const index_t unlinked_index = unlink_index(index);
				assert(unlinked_index == index);

				insert_empty(d, key, value);
				insert(victim_key, victim_value);
				return index;
			}
			else
			{
				return insert_tail(d, key, value);
			}
		}
		else
		{
			insert_empty(d, key, value);
			return index;
		}
	}

	index_t unlink_index(index_t index)
	{
		entry& e = get_entry(index);
		data& d = e.d;
		assert(e.is_data());
		// fix previous.
		if (d.prev != invalid_index)
		{
			get_entry(d.prev).d.next = d.next;
			if (d.next != invalid_index)
			{
				get_entry(d.next).d.prev = d.prev;
			}
		}
		else
		{
			// i'm the first, move the next to first and unlink the next.
			const index_t next_index = d.next;
			if (next_index != invalid_index)
			{
				data& next = get_entry(next_index).d;
				const index_t unlinked_index = unlink_index(next_index);
				assert(unlinked_index == next_index);

				d.key = next.key;
				d.value = next.value;
				
				index = unlinked_index;
			}
		}
		return index;
	}

	void remove_index(index_t index)
	{
		index = unlink_index(index);
		add_node(index);
	}

	void try_grow()
	{
		if (m_size >= m_entries_size)
		{
			rehash();
		}
	}

	void rehash()
	{
		fhash_table old_table(std::move(*this));

		m_entries_size = std::max(m_size * entries_per_bucket, 4);
		m_entries = malloc(m_entries_size * sizeof(entry));

		// build the tree.
		m_root = build_tree(0, m_entries_size);
		get_node(m_root).parent = invalid_index;

		// now insert old data to new table.
		if (old_table.m_entries != nullptr)
		{
			for (int32_t i = 0; i < old_table.m_entries_size; i++)
			{
				entry& e = old_table.get_entry(i);
				if (e.is_data())
				{
					insert_no_check(e.d.key, e.d.value);
				}
			}
		}
	}

private:

	entry& get_entry(index_t index)
	{
		return m_entries[index.value];
	}

	entry& get_entry(node_index_t node_index)
	{
		return get_node(node::node_index_to_index(node_index));
	}

	node& get_node(index_t index)
	{
		return get_entry(index).n;
	}

	node& get_node(node_index_t node_index)
	{
		return get_entry(node_index).n;
	}

	// size tree operation.
	index_t build_tree(index_t begin, index_t end)
	{
		if (begin == end)
		{
			return invalid_index;
		}
		const index_t mid = (begin + end) / index_t(2);
		node& root = get_node(mid);

		const index_t lchild = build_tree(begin, mid);
		const index_t rchild = build_tree(mid + 1, end);

		if (lchild != invalid_index)
		{
			get_node(lchild).parent = node::index_to_node_index(mid);
			root.lchild = node::index_to_node_index(lchild);
		}
		else
		{
			root.lchild = invalid_node_index;
		}

		if (rchild != invalid_index)
		{
			get_node(rchild).parent = node::index_to_node_index(mid);
			root.rchild = node::index_to_node_index(rchild);
		}
		else
		{
			root.rchild = invalid_node_index;
		}

		return mid;
	}

	index_t get_node_dir(node_index_t index)
	{
		node& n = get_node(index);
		if (n.parent == invalid_index)
		{
			return invalid_index;
		}
		node& p = get_node(n.parent);
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

	index_t step(index_t index, index_t dir)
	{
		node_index_t pchild = invalid_node_index;
		node_index_t child = get_node(index).get_child_index(dir);
		while (child != invalid_node_index)
		{
			pchild = child;
			child = get_node(child).get_child_index(1 - dir);
		}
		if (pchild == invalid_node_index)
		{
			return invalid_index;
		}
		else
		{
			return node_index_to_index(pchild);
		}
	}

	void swap_node_index(index_t a, index_t b)
	{
		node& na = get_node(a);
		node& nb = get_node(b);
		const node_index_t na_index = node::index_to_node_index(a);
		const node_index_t nb_index = node::index_to_node_index(b);

		// swap parent.child index.
		const index_t na_dir = get_node_dir(a);
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
		if (na.lchild != invalid_node_index)
		{
			get_node(na.lchild).parent = nb_index;
		}
		if (na.rchild != invalid_node_index)
		{
			get_node(na.rchild).parent = nb_index;
		}

		if (nb.lchild != invalid_node_index)
		{
			get_node(nb.lchild).parent = na_index;
		}
		if (nb.rchild != invalid_node_index)
		{
			get_node(nb.rchild).parent = na_index;
		}

		// swap node data
		std::swap(na.lchild, nb.lchild);
		std::swap(na.rchild, nb.rchild);
		std::swap(na.parent, nb.parent);
		// does not swap index, it's our data!
	}

	void remove_node(index_t index)
	{
		node& n = get_node(index);
		node_index_t new_root = invalid_node_index;

		if (n.lchild != invalid_node_index && n.rchild != invalid_node_index)
		{
			index_t swap_index = step<1>(index);
			assert(swap_index != invalid_index);
			swap_node_index(index, swap_index);
		}
		assert(n.lchild == invalid_node_index || n.rchild == invalid_node_index);

		if (n.lchild != invalid_node_index)
		{
			new_root = n.lchild;
		}
		else if (n.rchild != invalid_node_index)
		{
			new_root = n.rchild;
		}

		if (new_root != invalid_node_index)
		{
			const index_t node_dir = get_node_dir(index);
			if (node_dir != invalid_index)
			{
				get_node(n.parent).get_child_index(node_dir) = new_root;
			}
			get_node(new_root).parent = n.parent;
		}
		else if (n.parent == invalid_node_index)
		{
			m_root = invalid_index;
		}
	}

	index_t find(index_t index, index_t& last_dir) const
	{
		index_t prev = invalid_index;
		index_t current = m_root;
		while (current != invalid_index)
		{
			const node& n = get_node(current);
			index_t next = invalid_index;
			if (current == index)
			{
				return index;
			}
			if (n.lchild != invalid_index && index < node::node_index_to_index(n.lchild))
			{
				next = node::node_index_to_index(n.lchild);
				last_dir = index_t(0);
			}
			else
			{
				next = node::node_index_to_index(n.rchild);
				last_dir = index_t(1);
			}
			prev = current;
			current = next;
		}
		return prev;
	}

	void add_node(index_t index)
	{
		index_t last_dir;
		index_t insert_index = find(index, last_dir);
		assert(insert_index < index);
		if (insert_index == invalid_index)
		{
			assert(m_root == invalid_index);
			m_root = index;
			node& n = get_node(index);
			n.lchild = n.rchild = n.parent = invalid_node_index;
			return;
		}

		node& n = get_node(index);
		n.lchild = n.rchild = invalid_node_index;

		node_index_t& child_index = get_node(insert_index).get_child_index(last_dir);
		assert(child_index == invalid_node_index);
		child_index = node::index_to_node_index(index);
		n.parent = node::index_to_node_index(insert_index);
	}

	int32_t bucket_size() const
	{
		return m_entries_size / entries_per_bucket;
	}

private:
	entry* m_entries = nullptr;
	hasher_t m_hasher;
	int32_t m_entries_size = 0;
	int32_t m_size = 0;
	index_t m_root = invalid_index;
};
