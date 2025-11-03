/**
 * MIT License
 *
 * Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>
 * Copyright (c) 2023 Your Name <your.email@example.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef TSL_ORDERED_HASH_SORTED_H
#define TSL_ORDERED_HASH_SORTED_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "ordered_hash.h"
#include "ordered_map.h"
#include "ordered_set.h"

namespace tsl {

namespace detail_ordered_hash_sorted {

/**
 * Selector to get the key from a value_type
 */
template <class Key, class T>
struct key_selector {
    using key_type = Key;
    
    const key_type& operator()(const std::pair<const Key, T>& value) const {
        return value.first;
    }
};

/**
 * Selector to get the value from a value_type
 */
template <class Key, class T>
struct value_selector {
    using key_type = T;
    
    const key_type& operator()(const std::pair<const Key, T>& value) const {
        return value.second;
    }
};

/**
 * Selector to get the key from a value_type (for set)
 */
template <class Key>
struct set_key_selector {
    using key_type = Key;
    
    const key_type& operator()(const Key& value) const {
        return value;
    }
};

}

/**
 * ordered_hash with sorted view support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_hash_sorted : public tsl::ordered_hash<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_hash<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
public:
    using key_type = typename base::key_type;
    using mapped_type = typename base::mapped_type;
    using value_type = typename base::value_type;
    using size_type = typename base::size_type;
    using difference_type = typename base::difference_type;
    using hasher = typename base::hasher;
    using key_equal = typename base::key_equal;
    using allocator_type = typename base::allocator_type;
    using reference = typename base::reference;
    using const_reference = typename base::const_reference;
    using pointer = typename base::pointer;
    using const_pointer = typename base::const_pointer;
    using iterator = typename base::iterator;
    using const_iterator = typename base::const_iterator;
    using local_iterator = typename base::local_iterator;
    using const_local_iterator = typename base::const_local_iterator;
    
    // Inherit constructors
    using base::base;
    
    /**
     * Sort the elements based on the given comparator
     * 
     * The comparator should take two const value_type& parameters and return true
     * if the first should come before the second.
     * 
     * This will create a sorted view of the elements, but the underlying
     * hash table structure remains unchanged.
     */
    template <class Compare>
    void sort(Compare comp) {
        m_sorted_indices.clear();
        m_sorted_indices.reserve(base::size());
        
        for (size_type i = 0; i < base::size(); ++i) {
            m_sorted_indices.push_back(i);
        }
        
        std::sort(m_sorted_indices.begin(), m_sorted_indices.end(),
                  [this, &comp](size_type a, size_type b) {
                      return comp(base::m_values[a], base::m_values[b]);
                  });
    }
    
    /**
     * Sort the elements based on their keys using the given comparator
     * 
     * The comparator should take two const key_type& parameters and return true
     * if the first should come before the second.
     */
    template <class Compare>
    void sort_by_key(Compare comp) {
        detail_ordered_hash_sorted::key_selector<Key, T> selector;
        
        sort([&selector, &comp](const value_type& a, const value_type& b) {
            return comp(selector(a), selector(b));
        });
    }
    
    /**
     * Sort the elements based on their values using the given comparator
     * 
     * The comparator should take two const mapped_type& parameters and return true
     * if the first should come before the second.
     */
    template <class Compare>
    void sort_by_value(Compare comp) {
        detail_ordered_hash_sorted::value_selector<Key, T> selector;
        
        sort([&selector, &comp](const value_type& a, const value_type& b) {
            return comp(selector(a), selector(b));
        });
    }
    
    /**
     * Sort the elements in ascending order based on their keys
     */
    void sort_by_key() {
        sort_by_key(std::less<key_type>());
    }
    
    /**
     * Sort the elements in ascending order based on their values
     */
    void sort_by_value() {
        sort_by_value(std::less<mapped_type>());
    }
    
    /**
     * Check if the elements are currently sorted
     */
    bool is_sorted() const {
        return !m_sorted_indices.empty();
    }
    
    /**
     * Clear the sorted view
     */
    void clear_sorted() {
        m_sorted_indices.clear();
    }
    
    /**
     * Get the sorted begin iterator
     */
    const_iterator sorted_begin() const {
        tsl_oh_assert(is_sorted());
        return const_iterator(this, 0, true);
    }
    
    /**
     * Get the sorted end iterator
     */
    const_iterator sorted_end() const {
        tsl_oh_assert(is_sorted());
        return const_iterator(this, base::size(), true);
    }
    
    /**
     * Get the sorted rbegin iterator
     */
    const_reverse_iterator sorted_rbegin() const {
        return const_reverse_iterator(sorted_end());
    }
    
    /**
     * Get the sorted rend iterator
     */
    const_reverse_iterator sorted_rend() const {
        return const_reverse_iterator(sorted_begin());
    }
    
    // Override modify operations to clear sorted view
    std::pair<iterator, bool> insert(const value_type& value) {
        clear_sorted();
        return base::insert(value);
    }
    
    std::pair<iterator, bool> insert(value_type&& value) {
        clear_sorted();
        return base::insert(std::move(value));
    }
    
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        clear_sorted();
        return base::emplace(std::forward<Args>(args)...);
    }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        clear_sorted();
        base::insert(first, last);
    }
    
    void insert(std::initializer_list<value_type> ilist) {
        clear_sorted();
        base::insert(ilist);
    }
    
    iterator erase(iterator pos) {
        clear_sorted();
        return base::erase(pos);
    }
    
    iterator erase(const_iterator pos) {
        clear_sorted();
        return base::erase(pos);
    }
    
    iterator erase(iterator first, iterator last) {
        clear_sorted();
        return base::erase(first, last);
    }
    
    iterator erase(const_iterator first, const_iterator last) {
        clear_sorted();
        return base::erase(first, last);
    }
    
    size_type erase(const key_type& key) {
        clear_sorted();
        return base::erase(key);
    }
    
    template <class K>
    size_type erase(const K& key) {
        clear_sorted();
        return base::erase(key);
    }
    
    void clear() noexcept {
        clear_sorted();
        base::clear();
    }
    
    void swap(ordered_hash_sorted& other) noexcept {
        using std::swap;
        base::swap(other);
        swap(m_sorted_indices, other.m_sorted_indices);
    }
    
private:
    friend class const_iterator;
    
    // Override const_iterator to support sorted view
    class const_iterator : public base::const_iterator {
    public:
        using base_iterator = typename base::const_iterator;
        using difference_type = typename base_iterator::difference_type;
        using value_type = typename base_iterator::value_type;
        using pointer = typename base_iterator::pointer;
        using reference = typename base_iterator::reference;
        using iterator_category = typename base_iterator::iterator_category;
        
        const_iterator(const ordered_hash_sorted* ht, size_type index, bool is_sorted)
            : base_iterator(ht, index), m_ht(ht), m_is_sorted(is_sorted) {}
            
        reference operator*() const {
            if (m_is_sorted) {
                return m_ht->m_values[m_ht->m_sorted_indices[m_index]];
            } else {
                return base_iterator::operator*();
            }
        }
        
        pointer operator->() const {
            if (m_is_sorted) {
                return &m_ht->m_values[m_ht->m_sorted_indices[m_index]];
            } else {
                return base_iterator::operator->();
            }
        }
        
        const_iterator& operator++() {
            ++m_index;
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++m_index;
            return tmp;
        }
        
        const_iterator& operator--() {
            --m_index;
            return *this;
        }
        
        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --m_index;
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            return m_ht == other.m_ht && m_index == other.m_index && m_is_sorted == other.m_is_sorted;
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
        
    private:
        const ordered_hash_sorted* m_ht;
        bool m_is_sorted;
    };
    
    // Override reverse_iterator to support sorted view
    class const_reverse_iterator : public std::reverse_iterator<const_iterator> {
    public:
        using base_reverse_iterator = std::reverse_iterator<const_iterator>;
        
        explicit const_reverse_iterator(const_iterator it) : base_reverse_iterator(it) {}
    };
    
    std::vector<size_type> m_sorted_indices;
};

/**
 * ordered_map with sorted view support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_map_sorted : public tsl::ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    using hash_type = ordered_hash_sorted<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
public:
    using key_type = typename base::key_type;
    using mapped_type = typename base::mapped_type;
    using value_type = typename base::value_type;
    using size_type = typename base::size_type;
    using difference_type = typename base::difference_type;
    using hasher = typename base::hasher;
    using key_equal = typename base::key_equal;
    using allocator_type = typename base::allocator_type;
    using reference = typename base::reference;
    using const_reference = typename base::const_reference;
    using pointer = typename base::pointer;
    using const_pointer = typename base::const_pointer;
    using iterator = typename base::iterator;
    using const_iterator = typename base::const_iterator;
    using local_iterator = typename base::local_iterator;
    using const_local_iterator = typename base::const_local_iterator;
    using const_reverse_iterator = typename hash_type::const_reverse_iterator;
    
    // Inherit constructors
    using base::base;
    
    /**
     * Sort the elements based on the given comparator
     * 
     * The comparator should take two const value_type& parameters and return true
     * if the first should come before the second.
     * 
     * This will create a sorted view of the elements, but the underlying
     * hash table structure remains unchanged.
     */
    template <class Compare>
    void sort(Compare comp) {
        static_cast<hash_type&>(base::m_ht).sort(comp);
    }
    
    /**
     * Sort the elements based on their keys using the given comparator
     * 
     * The comparator should take two const key_type& parameters and return true
     * if the first should come before the second.
     */
    template <class Compare>
    void sort_by_key(Compare comp) {
        static_cast<hash_type&>(base::m_ht).sort_by_key(comp);
    }
    
    /**
     * Sort the elements based on their values using the given comparator
     * 
     * The comparator should take two const mapped_type& parameters and return true
     * if the first should come before the second.
     */
    template <class Compare>
    void sort_by_value(Compare comp) {
        static_cast<hash_type&>(base::m_ht).sort_by_value(comp);
    }
    
    /**
     * Sort the elements in ascending order based on their keys
     */
    void sort_by_key() {
        static_cast<hash_type&>(base::m_ht).sort_by_key();
    }
    
    /**
     * Sort the elements in ascending order based on their values
     */
    void sort_by_value() {
        static_cast<hash_type&>(base::m_ht).sort_by_value();
    }
    
    /**
     * Check if the elements are currently sorted
     */
    bool is_sorted() const {
        return static_cast<const hash_type&>(base::m_ht).is_sorted();
    }
    
    /**
     * Clear the sorted view
     */
    void clear_sorted() {
        static_cast<hash_type&>(base::m_ht).clear_sorted();
    }
    
    /**
     * Get the sorted begin iterator
     */
    const_iterator sorted_begin() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_begin();
    }
    
    /**
     * Get the sorted end iterator
     */
    const_iterator sorted_end() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_end();
    }
    
    /**
     * Get the sorted rbegin iterator
     */
    const_reverse_iterator sorted_rbegin() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_rbegin();
    }
    
    /**
     * Get the sorted rend iterator
     */
    const_reverse_iterator sorted_rend() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_rend();
    }
};

/**
 * ordered_set with sorted view support
 */
template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key>,
          class IndexType = std::uint32_t>
class ordered_set_sorted : public tsl::ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    using hash_type = ordered_hash_sorted<Key, Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
public:
    using key_type = typename base::key_type;
    using value_type = typename base::value_type;
    using size_type = typename base::size_type;
    using difference_type = typename base::difference_type;
    using hasher = typename base::hasher;
    using key_equal = typename base::key_equal;
    using allocator_type = typename base::allocator_type;
    using reference = typename base::reference;
    using const_reference = typename base::const_reference;
    using pointer = typename base::pointer;
    using const_pointer = typename base::const_pointer;
    using iterator = typename base::iterator;
    using const_iterator = typename base::const_iterator;
    using local_iterator = typename base::local_iterator;
    using const_local_iterator = typename base::const_local_iterator;
    using const_reverse_iterator = typename hash_type::const_reverse_iterator;
    
    // Inherit constructors
    using base::base;
    
    /**
     * Sort the elements based on the given comparator
     * 
     * The comparator should take two const value_type& parameters and return true
     * if the first should come before the second.
     * 
     * This will create a sorted view of the elements, but the underlying
     * hash table structure remains unchanged.
     */
    template <class Compare>
    void sort(Compare comp) {
        static_cast<hash_type&>(base::m_ht).sort(comp);
    }
    
    /**
     * Sort the elements based on their keys using the given comparator
     * 
     * The comparator should take two const key_type& parameters and return true
     * if the first should come before the second.
     */
    template <class Compare>
    void sort_by_key(Compare comp) {
        static_cast<hash_type&>(base::m_ht).sort_by_key(comp);
    }
    
    /**
     * Sort the elements in ascending order based on their keys
     */
    void sort_by_key() {
        static_cast<hash_type&>(base::m_ht).sort_by_key();
    }
    
    /**
     * Check if the elements are currently sorted
     */
    bool is_sorted() const {
        return static_cast<const hash_type&>(base::m_ht).is_sorted();
    }
    
    /**
     * Clear the sorted view
     */
    void clear_sorted() {
        static_cast<hash_type&>(base::m_ht).clear_sorted();
    }
    
    /**
     * Get the sorted begin iterator
     */
    const_iterator sorted_begin() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_begin();
    }
    
    /**
     * Get the sorted end iterator
     */
    const_iterator sorted_end() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_end();
    }
    
    /**
     * Get the sorted rbegin iterator
     */
    const_reverse_iterator sorted_rbegin() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_rbegin();
    }
    
    /**
     * Get the sorted rend iterator
     */
    const_reverse_iterator sorted_rend() const {
        return static_cast<const hash_type&>(base::m_ht).sorted_rend();
    }
};

} // namespace tsl

#endif // TSL_ORDERED_HASH_SORTED_H