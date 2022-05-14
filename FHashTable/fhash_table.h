#pragma once
#include <stdint.h>
#include <assert.h>
#include <memory>
#include <vector>

template <typename key_t, typename value_t, typename hasher_t = std::hash<key_t>, int32_t entries_per_bucket = 2>
class fhash_table
{
public:
	template <typename raw_integer_t, typename tag>
	struct integer_t
	{
		raw_integer_t value;
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
		friend integer_t operator - (integer_t a, integer_t b) { return integer_t(a.value - b.value); }
		friend integer_t operator * (integer_t a, integer_t b) { return integer_t(a.value * b.value); }
		friend integer_t operator / (integer_t a, integer_t b) { return integer_t(a.value / b.value); }
	};

	struct tag_index {};
	struct tag_node_index {};
	struct tag_hash {};

	using index_t = integer_t<int32_t, tag_index>;
	using node_index_t = integer_t<int32_t, tag_node_index>;
	using hash_t = integer_t<size_t, tag_hash>;

	static constexpr index_t invalid_index = index_t(-1);

	static constexpr node_index_t invalid_node_index = node_index_t(-2);

	static constexpr int32_t min_entries_size = 4;

	// node index start from -3 to -inf.
	// index + node_index + 3 = 0.
	static index_t node_index_to_index(node_index_t node_index)
	{
		return index_t(-3 - node_index.value);
	}

	static index_t node_index_to_index_check_invalid(node_index_t node_index)
	{
		if (node_index == invalid_node_index)
		{
			return invalid_index;
		}
		else
		{
			return node_index_to_index(node_index);
		}
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
		other.m_entries = get_default_entries();

		m_entries_size = other.m_entries_size;
		other.m_entries_size = min_entries_size;

		m_size = other.m_size; 
		other.m_size = 0;

		m_root = other.m_root;
		other.m_root = invalid_index;
	}

	~fhash_table()
	{
		clear();
	}

	void clear()
	{
		if (m_entries != get_default_entries())
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
			m_entries = get_default_entries();
		}
		m_entries_size = min_entries_size;
		m_size = 0;
		m_root = invalid_index;
	}

	const value_t* find(key_t key) const
	{
		return find_index(key, compute_slot(compute_hash(key)), 
			[this](index_t index) {return (const value_t*)&get_entry(index).d.value; },
			[]() {return (const value_t*)nullptr; }
			);
	}

	index_t insert(key_t key, value_t value)
	{
		const hash_t hash = compute_hash(key);
		if (m_size > 0)
		{
			const bool found = find_index(key, compute_slot(hash), 
				[](index_t index) {return true; },
				[]() {return false; });
			if (found)
			{
				// alread exists.
				return invalid_index;
			}
		}
		try_grow();
		const index_t index = insert_index_no_check(compute_slot(hash), key, value);
		return index;
	}

	bool remove(key_t key)
	{
		return find_index(key, compute_slot(compute_hash(key)), 
			[this](index_t index) {remove_index(index); return true; },
			[]() {return false; });
	}

	void validate() const
	{
		if (m_entries != get_default_entries())
		{
			std::vector<bool> visited;
			visited.resize(m_entries_size);
			int32_t size = 0;
			int32_t visited_size = 0;
			for (int32_t i = 0; i < m_entries_size; i++)
			{
				index_t index = index_t(i);
				const entry* e = &get_entry(index);
				if (e->is_data())
				{
					if (e->d.prev == invalid_index)
					{
						for (; index != invalid_index; index = e->d.next, e = &get_entry(index))
						{
							assert(!visited[index.value]);
							if (e->d.prev != invalid_index)
							{
								assert(get_entry(e->d.prev).d.next == index);
							}
							if (e->d.next != invalid_index)
							{
								assert(get_entry(e->d.next).d.prev == index);
							}
							visited[index.value] = true;
							visited_size++;
						}
					}
					size++;
				}
			}

			assert(size == visited_size);
			assert(size == m_size);
			
			int32_t tree_size = validate_tree(m_root);
			assert(tree_size + m_size == m_entries_size);
		}
	}

	void reserve(int32_t expected_size)
	{
		if (expected_size > get_alloctable_entries_size() / entries_per_bucket)
		{
			rehash(expected_size);
		}
	}

private:

	int32_t validate_tree(index_t index) const
	{
		if (index == invalid_index)
		{
			return 0;
		}
		int32_t size = 1;
		const node& n = get_node(index);
		if (n.lchild != invalid_node_index)
		{
			assert(get_node(n.lchild).parent == index_to_node_index(index));
			size += validate_tree(node_index_to_index(n.lchild));
		}
		if (n.rchild != invalid_node_index)
		{
			assert(get_node(n.rchild).parent == index_to_node_index(index));
			size += validate_tree(node_index_to_index(n.rchild));
		}
		return size;
	}

	hash_t compute_hash(key_t key) const
	{
		return hash_t(m_hasher(key));
	}

	index_t compute_slot(hash_t h) const
	{
		return index_t(h.value % bucket_size());
	}

	void insert_empty(data& d, key_t key, value_t value)
	{
		new (&d.key) key_t(key);
		new (&d.value) value_t(value);
		d.next = invalid_index;
		d.prev = invalid_index;
	}

	index_t allocate_entry(index_t index)
	{
		index_t last_dir;
		index_t pos = find_node(index, last_dir);
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
		new (&t.key) key_t(key);
		new (&t.value) value_t(value);
		return new_index;
	}

	index_t insert_index_no_check(index_t index, key_t key, value_t value)
	{
		entry& e = get_entry(index);
		data& d = e.d;
		if (e.is_data())
		{
			if (d.prev != invalid_index)
			{
				// we are list from other slot.
				key_t victim_key = d.key;
				value_t victim_value = d.value;

				const index_t unlinked_index = unlink_index(index);
				assert(unlinked_index == index);
				insert_empty(d, key, value);

				insert_index_no_check(compute_slot(compute_hash(key)), victim_key, victim_value);
				return index;
			}
			else
			{
				m_size++;
				return insert_tail(index, key, value);
			}
		}
		else
		{
			m_size++;
			remove_node(index);
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

				d.key = std::move(next.key);
				d.value = std::move(next.value);
				
				index = unlinked_index;
			}
		}
		return index;
	}

	void remove_index(index_t index)
	{
		index = unlink_index(index);
		entry& e = get_entry(index);
		assert(e.is_data());
		e.d.key.~key_t();
		e.d.value.~value_t();
		add_node(index);
		m_size--;
	}

	template <typename success_operation_t, typename failed_operation_t>
	decltype(auto) find_index(key_t key, index_t index, success_operation_t success_operation, failed_operation_t failed_operation) const
	{
		const entry* e = &get_entry(index);
		if (!e->is_data())
		{
			return failed_operation();
		}
		for (; index != invalid_index; index = e->d.next, e = &get_entry(index))
		{
			if (e->d.key == key)
			{
				return success_operation(index);
			}
		}
		return failed_operation();
	}

	void try_grow()
	{
		if (m_size + 1 >= get_alloctable_entries_size())
		{
			rehash(m_size + 1);
		}
	}

	void rehash(int32_t expected_size)
	{
		fhash_table old_table(std::move(*this));
		m_entries_size = std::max(expected_size * entries_per_bucket, min_entries_size);
		// rehash shoudn't throw any data.
		m_entries_size = std::max(m_entries_size, m_size);

		m_entries = (entry*)malloc(m_entries_size * sizeof(entry));

		// build the tree.
		m_root = build_tree(index_t(0), index_t(m_entries_size));
		get_node(m_root).parent = invalid_node_index;

		// now insert old data to new table.
		if (old_table.m_entries != get_default_entries())
		{
			for (int32_t i = 0; i < old_table.m_entries_size; i++)
			{
				entry& e = old_table.get_entry(index_t(i));
				if (e.is_data())
				{
					insert_index_no_check(compute_slot(compute_hash(e.d.key)), e.d.key, e.d.value);
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
		return get_entry(node_index_to_index(node_index));
	}

	const entry& get_entry(index_t index) const
	{
		return m_entries[index.value];
	}

	const entry& get_entry(node_index_t node_index) const
	{
		return get_entry(node_index_to_index(node_index));
	}

	node& get_node(index_t index)
	{
		return get_entry(index).n;
	}

	node& get_node(node_index_t node_index)
	{
		return get_entry(node_index).n;
	}

	const node& get_node(index_t index) const
	{
		return get_entry(index).n;
	}

	const node& get_node(node_index_t node_index) const
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
		const index_t rchild = build_tree(mid + index_t(1), end);

		if (lchild != invalid_index)
		{
			get_node(lchild).parent = index_to_node_index(mid);
			root.lchild = index_to_node_index(lchild);
		}
		else
		{
			root.lchild = invalid_node_index;
		}

		if (rchild != invalid_index)
		{
			get_node(rchild).parent = index_to_node_index(mid);
			root.rchild = index_to_node_index(rchild);
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
		if (n.parent == invalid_node_index)
		{
			return invalid_index;
		}
		node& p = get_node(n.parent);
		if (p.lchild == index)
		{
			return index_t(0);
		}
		else if (p.rchild == index)
		{
			return index_t(1);
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
			child = get_node(child).get_child_index(index_t(1) - dir);
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

	void remove_node(index_t erased_index)
	{
		const node_index_t erased_node_index = index_to_node_index(erased_index);

		// copied from std::_Tree_val::_Extract
		// TODO optimize these messes.
		node_index_t erased_node = erased_node_index;
		node_index_t fixnode;
		node_index_t fixnode_parent;
		node_index_t pnode = erased_node;
		if (get_node(pnode).lchild == invalid_node_index)
		{
			fixnode = get_node(pnode).rchild;
		}
		else if (get_node(pnode).rchild == invalid_node_index)
		{
			fixnode = get_node(pnode).lchild;
		}
		else
		{
			pnode = index_to_node_index(step(erased_index, index_t(1)));
			fixnode = get_node(pnode).rchild;
		}

		if (pnode == erased_node)
		{
			fixnode_parent = get_node(erased_node).parent;
			if (fixnode != invalid_node_index)
			{
				get_node(fixnode).parent = fixnode_parent;
			}
			if (m_root == node_index_to_index(erased_node))
			{
				m_root = node_index_to_index(fixnode);
			}
			else if (get_node(fixnode_parent).lchild == erased_node_index)
			{
				get_node(fixnode_parent).lchild = fixnode;
			}
			else
			{
				get_node(fixnode_parent).rchild = fixnode;
			}
		}
		else
		{
			get_node(get_node(erased_node).lchild).parent = pnode;
			get_node(pnode).lchild = get_node(erased_node).lchild;
			if (pnode == get_node(erased_node).rchild)
			{
				fixnode_parent = pnode;
			}
			else
			{
				fixnode_parent = get_node(pnode).parent;
				if (fixnode != invalid_node_index)
				{
					get_node(fixnode).parent = fixnode_parent;
				}
				get_node(fixnode_parent).lchild = fixnode;
				get_node(pnode).rchild = get_node(erased_node).rchild;
				get_node(get_node(erased_node).rchild).parent = pnode;
			}
			if (m_root == node_index_to_index(erased_node))
			{
				m_root = node_index_to_index(pnode);
			}
			else if (get_node(get_node(erased_node).parent).lchild == erased_node)
			{
				get_node(get_node(erased_node).parent).lchild = pnode;
			}
			else
			{
				get_node(get_node(erased_node).parent).rchild = pnode;
			}
			get_node(pnode).parent = get_node(erased_node).parent;
		}
	}

	index_t find_node(index_t index, index_t& last_dir) const
	{
		index_t prev = invalid_index;
		index_t current = m_root;
		while (current != invalid_index)
		{
			const node& n = get_node(current);
			prev = current;
			if (index == current)
			{
				return index;
			}
			else if (index < current)
			{
				current = node_index_to_index_check_invalid(n.lchild);
				last_dir = index_t(0);
			}
			else
			{
				current = node_index_to_index_check_invalid(n.rchild);
				last_dir = index_t(1);
			}
		}
		return prev;
	}

	void add_node(index_t index)
	{
		index_t last_dir;
		index_t insert_index = find_node(index, last_dir);
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
		child_index = index_to_node_index(index);
		n.parent = index_to_node_index(insert_index);
	}

	int32_t bucket_size() const
	{
		return m_entries_size / entries_per_bucket;
	}

	struct default_entries_initializer
	{
		entry entries[min_entries_size];
		default_entries_initializer()
		{
			for (int i = 0; i < min_entries_size; i++)
			{
				node& n = entries[i].n;
				n.lchild = n.rchild = n.parent = invalid_node_index;
			}
		}
	};

	static entry* get_default_entries()
	{
		static default_entries_initializer default_entries;
		return default_entries.entries;
	}

	int32_t get_alloctable_entries_size() const
	{
		return m_entries == get_default_entries() ? 0 : m_entries_size;
	}

private:
	entry* m_entries = get_default_entries();
	hasher_t m_hasher;
	int32_t m_entries_size = min_entries_size;
	int32_t m_size = 0;
	index_t m_root = invalid_index;
};
