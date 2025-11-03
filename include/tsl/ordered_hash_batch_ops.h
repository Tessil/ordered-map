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
#ifndef TSL_ORDERED_HASH_BATCH_OPS_H
#define TSL_ORDERED_HASH_BATCH_OPS_H

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

namespace tsl {

namespace detail_ordered_hash_batch {

/**
 * Helper struct to store precomputed hash and value for batch insertion
 */
template <class ValueType, class HashType>
struct precomputed_hash_value {
    HashType hash;
    ValueType value;
    
    precomputed_hash_value(HashType hash, ValueType value)
        : hash(hash), value(std::move(value)) {}
};

/**
 * Compare two precomputed_hash_value by their hash
 */
template <class ValueType, class HashType>
struct compare_precomputed_hash {
    bool operator()(const precomputed_hash_value<ValueType, HashType>& a, 
                    const precomputed_hash_value<ValueType, HashType>& b) const {
        return a.hash < b.hash;
    }
};

}

/**
 * ordered_hash with batch operations support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_hash_batch : public tsl::ordered_hash<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_hash<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    using precomputed_hash_value = detail_ordered_hash_batch::precomputed_hash_value<typename base::value_type, typename base::hash_type>;
    
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
     * Batch insert a range of elements
     * 
     * This method optimizes the insertion of multiple elements by:
     * 1. Precomputing all hashes
     * 2. Sorting the elements by hash to minimize conflicts
     * 3. Reserving space in advance
     * 4. Inserting elements in hash order
     * 
     * Returns the number of elements inserted
     */
    template <class InputIt>
    size_type insert_batch(InputIt first, InputIt last) {
        const size_type count = std::distance(first, last);
        if (count == 0) {
            return 0;
        }
        
        // Precompute hashes and store values
        std::vector<precomputed_hash_value> precomputed;
        precomputed.reserve(count);
        
        for (InputIt it = first; it != last; ++it) {
            const key_type& key = base::key_select()(*it);
            const typename base::hash_type hash = base::hash_function()(key);
            
            precomputed.emplace_back(hash, *it);
        }
        
        // Sort by hash to minimize conflicts
        std::sort(precomputed.begin(), precomputed.end(), 
                  detail_ordered_hash_batch::compare_precomputed_hash<typename base::value_type, typename base::hash_type>());
        
        // Reserve space
        reserve(base::size() + count);
        
        // Insert elements
        size_type inserted = 0;
        for (const auto& p : precomputed) {
            if (base::insert(p.value).second) {
                inserted++;
            }
        }
        
        return inserted;
    }
    
    /**
     * Batch insert an initializer list of elements
     * 
     * Returns the number of elements inserted
     */
    size_type insert_batch(std::initializer_list<value_type> ilist) {
        return insert_batch(ilist.begin(), ilist.end());
    }
    
    /**
     * Batch erase a range of keys
     * 
     * This method optimizes the erasure of multiple keys by:
     * 1. Precomputing all hashes
     * 2. Sorting the keys by hash to minimize cache misses
     * 3. Using a more efficient erase implementation for sorted keys
     * 
     * Returns the number of elements erased
     */
    template <class InputIt>
    size_type erase_batch(InputIt first, InputIt last) {
        const size_type count = std::distance(first, last);
        if (count == 0) {
            return 0;
        }
        
        // Precompute hashes and store keys
        std::vector<std::pair<typename base::hash_type, key_type>> precomputed;
        precomputed.reserve(count);
        
        for (InputIt it = first; it != last; ++it) {
            const key_type& key = *it;
            const typename base::hash_type hash = base::m_hash(key);
            
            precomputed.emplace_back(hash, key);
        }
        
        // Sort by hash to minimize cache misses
        std::sort(precomputed.begin(), precomputed.end());
        
        // Erase elements
        size_type erased = 0;
        for (const auto& p : precomputed) {
            if (base::erase_impl(p.second, p.first)) {
                erased++;
            }
        }
        
        return erased;
    }
    
    /**
     * Batch erase an initializer list of keys
     * 
     * Returns the number of elements erased
     */
    size_type erase_batch(std::initializer_list<key_type> ilist) {
        return erase_batch(ilist.begin(), ilist.end());
    }
    
    /**
     * Batch update elements
     * 
     * This method optimizes the update of multiple elements by:
     * 1. Precomputing all hashes
     * 2. Sorting the elements by hash to minimize cache misses
     * 3. Finding all elements first, then updating them
     * 
     * The `Updater` should be a function object that takes a value_type& and returns void
     * 
     * Returns the number of elements updated
     */
    template <class InputIt, class Updater>
    size_type update_batch(InputIt first, InputIt last, Updater updater) {
        const size_type count = std::distance(first, last);
        if (count == 0) {
            return 0;
        }
        
        // Precompute hashes and store keys
        std::vector<std::pair<typename base::hash_type, key_type>> precomputed;
        precomputed.reserve(count);
        
        for (InputIt it = first; it != last; ++it) {
            const key_type& key = *it;
            const typename base::hash_type hash = base::m_hash(key);
            
            precomputed.emplace_back(hash, key);
        }
        
        // Sort by hash to minimize cache misses
        std::sort(precomputed.begin(), precomputed.end());
        
        // Update elements
        size_type updated = 0;
        for (const auto& p : precomputed) {
            iterator it = base::find_impl(p.second, p.first);
            if (it != base::end()) {
                updater(*it);
                updated++;
            }
        }
        
        return updated;
    }
    
    /**
     * Batch update an initializer list of keys
     * 
     * Returns the number of elements updated
     */
    template <class Updater>
    size_type update_batch(std::initializer_list<key_type> ilist, Updater updater) {
        return update_batch(ilist.begin(), ilist.end(), updater);
    }
    
    /**
     * Batch emplace elements
     * 
     * This method optimizes the emplacement of multiple elements by:
     * 1. Precomputing all hashes
     * 2. Sorting the elements by hash to minimize conflicts
     * 3. Reserving space in advance
     * 4. Emplacing elements in hash order
     * 
     * Returns the number of elements emplaced
     */
    template <class... Args>
    size_type emplace_batch(std::initializer_list<std::tuple<Args...>> ilist) {
        return emplace_batch(ilist.begin(), ilist.end());
    }
    
    /**
     * Batch emplace elements from a range
     * 
     * Returns the number of elements emplaced
     */
    template <class InputIt>
    size_type emplace_batch(InputIt first, InputIt last) {
        const size_type count = std::distance(first, last);
        if (count == 0) {
            return 0;
        }
        
        // Precompute hashes and store arguments
        std::vector<std::pair<typename base::hash_type, std::tuple<typename std::decay<Args>::type...>>> precomputed;
        precomputed.reserve(count);
        
        for (InputIt it = first; it != last; ++it) {
            const auto& args = *it;
            const key_type& key = std::get<0>(args);
            const typename base::hash_type hash = base::m_hash(key);
            
            precomputed.emplace_back(hash, args);
        }
        
        // Sort by hash to minimize conflicts
        std::sort(precomputed.begin(), precomputed.end(), 
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Reserve space
        reserve(base::size() + count);
        
        // Emplace elements
        size_type emplaced = 0;
        for (const auto& p : precomputed) {
            if (base::emplace_impl(p.second, p.first)) {
                emplaced++;
            }
        }
        
        return emplaced;
    }
};

/**
 * ordered_map with batch operations support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_map_batch : public tsl::ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    using hash_type = ordered_hash_batch<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
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
     * Batch insert a range of elements
     * 
     * Returns the number of elements inserted
     */
    template <class InputIt>
    size_type insert_batch(InputIt first, InputIt last) {
        return static_cast<hash_type&>(base::m_ht).insert_batch(first, last);
    }
    
    /**
     * Batch insert an initializer list of elements
     * 
     * Returns the number of elements inserted
     */
    size_type insert_batch(std::initializer_list<value_type> ilist) {
        return static_cast<hash_type&>(base::m_ht).insert_batch(ilist);
    }
    
    /**
     * Batch erase a range of keys
     * 
     * Returns the number of elements erased
     */
    template <class InputIt>
    size_type erase_batch(InputIt first, InputIt last) {
        return static_cast<hash_type&>(base::m_ht).erase_batch(first, last);
    }
    
    /**
     * Batch erase an initializer list of keys
     * 
     * Returns the number of elements erased
     */
    size_type erase_batch(std::initializer_list<key_type> ilist) {
        return static_cast<hash_type&>(base::m_ht).erase_batch(ilist);
    }
    
    /**
     * Batch update elements
     * 
     * The `Updater` should be a function object that takes a value_type& and returns void
     * 
     * Returns the number of elements updated
     */
    template <class InputIt, class Updater>
    size_type update_batch(InputIt first, InputIt last, Updater updater) {
        return static_cast<hash_type&>(base::m_ht).update_batch(first, last, updater);
    }
    
    /**
     * Batch update an initializer list of keys
     * 
     * Returns the number of elements updated
     */
    template <class Updater>
    size_type update_batch(std::initializer_list<key_type> ilist, Updater updater) {
        return static_cast<hash_type&>(base::m_ht).update_batch(ilist, updater);
    }
    
    /**
     * Batch emplace elements
     * 
     * Returns the number of elements emplaced
     */
    template <class... Args>
    size_type emplace_batch(std::initializer_list<std::tuple<Args...>> ilist) {
        return static_cast<hash_type&>(base::m_ht).emplace_batch(ilist);
    }
    
    /**
     * Batch emplace elements from a range
     * 
     * Returns the number of elements emplaced
     */
    template <class InputIt>
    size_type emplace_batch(InputIt first, InputIt last) {
        return static_cast<hash_type&>(base::m_ht).emplace_batch(first, last);
    }
};

/**
 * ordered_set with batch operations support
 */
template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key>,
          class IndexType = std::uint32_t>
class ordered_set_batch : public tsl::ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    using hash_type = ordered_hash_batch<Key, Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
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
    
    // Inherit constructors
    using base::base;
    
    /**
     * Batch insert a range of elements
     * 
     * Returns the number of elements inserted
     */
    template <class InputIt>
    size_type insert_batch(InputIt first, InputIt last) {
        return static_cast<hash_type&>(base::m_ht).insert_batch(first, last);
    }
    
    /**
     * Batch insert an initializer list of elements
     * 
     * Returns the number of elements inserted
     */
    size_type insert_batch(std::initializer_list<value_type> ilist) {
        return static_cast<hash_type&>(base::m_ht).insert_batch(ilist);
    }
    
    /**
     * Batch erase a range of keys
     * 
     * Returns the number of elements erased
     */
    template <class InputIt>
    size_type erase_batch(InputIt first, InputIt last) {
        return static_cast<hash_type&>(base::m_ht).erase_batch(first, last);
    }
    
    /**
     * Batch erase an initializer list of keys
     * 
     * Returns the number of elements erased
     */
    size_type erase_batch(std::initializer_list<key_type> ilist) {
        return static_cast<hash_type&>(base::m_ht).erase_batch(ilist);
    }
    
    /**
     * Batch update elements
     * 
     * The `Updater` should be a function object that takes a value_type& and returns void
     * 
     * Returns the number of elements updated
     */
    template <class InputIt, class Updater>
    size_type update_batch(InputIt first, InputIt last, Updater updater) {
        return static_cast<hash_type&>(base::m_ht).update_batch(first, last, updater);
    }
    
    /**
     * Batch update an initializer list of keys
     * 
     * Returns the number of elements updated
     */
    template <class Updater>
    size_type update_batch(std::initializer_list<key_type> ilist, Updater updater) {
        return static_cast<hash_type&>(base::m_ht).update_batch(ilist, updater);
    }
};

} // namespace tsl

#endif // TSL_ORDERED_HASH_BATCH_OPS_H