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
#ifndef TSL_ORDERED_MAP_THREAD_SAFE_H
#define TSL_ORDERED_MAP_THREAD_SAFE_H

#include <shared_mutex>
#include <type_traits>
#include "ordered_map.h"

namespace tsl {

/**
 * Thread-safe wrapper for ordered_map using a shared_mutex (C++17).
 *
 * This wrapper allows multiple threads to read concurrently while
 * only one thread can write at a time. It provides the same interface
 * as ordered_map, but with lock guards around each operation.
 */
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<Key, T>, Allocator>,
          class IndexType = std::uint_least32_t,
          class Mutex = std::shared_mutex>
class ordered_map_thread_safe {
public:
    // Types
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    
    // Iterators are not exposed as they would break thread safety
    
    // Constructors
    ordered_map_thread_safe() = default;
    
    explicit ordered_map_thread_safe(size_type bucket_count,
                                     const Hash& hash = Hash(),
                                     const KeyEqual& equal = KeyEqual(),
                                     const Allocator& alloc = Allocator())
        : m_map(bucket_count, hash, equal, alloc) {}
    
    ordered_map_thread_safe(size_type bucket_count, const Allocator& alloc)
        : m_map(bucket_count, alloc) {}
    
    ordered_map_thread_safe(size_type bucket_count, const Hash& hash,
                           const Allocator& alloc)
        : m_map(bucket_count, hash, alloc) {}
    
    explicit ordered_map_thread_safe(const Allocator& alloc)
        : m_map(alloc) {}
    
    template <class InputIt>
    ordered_map_thread_safe(InputIt first, InputIt last,
                           size_type bucket_count = ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual(),
                           const Allocator& alloc = Allocator())
        : m_map(first, last, bucket_count, hash, equal, alloc) {}
    
    template <class InputIt>
    ordered_map_thread_safe(InputIt first, InputIt last, size_type bucket_count,
                           const Allocator& alloc)
        : m_map(first, last, bucket_count, alloc) {}
    
    template <class InputIt>
    ordered_map_thread_safe(InputIt first, InputIt last, size_type bucket_count,
                           const Hash& hash, const Allocator& alloc)
        : m_map(first, last, bucket_count, hash, alloc) {}
    
    ordered_map_thread_safe(std::initializer_list<value_type> init,
                           size_type bucket_count = ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual(),
                           const Allocator& alloc = Allocator())
        : m_map(init, bucket_count, hash, equal, alloc) {}
    
    ordered_map_thread_safe(std::initializer_list<value_type> init, size_type bucket_count,
                           const Allocator& alloc)
        : m_map(init, bucket_count, alloc) {}
    
    ordered_map_thread_safe(std::initializer_list<value_type> init, size_type bucket_count,
                           const Hash& hash, const Allocator& alloc)
        : m_map(init, bucket_count, hash, alloc) {}
    
    // Assignment operators
    ordered_map_thread_safe& operator=(std::initializer_list<value_type> ilist) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map = ilist;
        return *this;
    }
    
    ordered_map_thread_safe& operator=(const ordered_map_thread_safe& other) {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::shared_lock<Mutex> lock_other(other.m_mutex);
            m_map = other.m_map;
        }
        return *this;
    }
    
    ordered_map_thread_safe& operator=(ordered_map_thread_safe&& other) noexcept {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::unique_lock<Mutex> lock_other(other.m_mutex);
            m_map = std::move(other.m_map);
        }
        return *this;
    }
    
    // Capacity
    bool empty() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.empty();
    }
    
    size_type size() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.size();
    }
    
    size_type max_size() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.max_size();
    }
    
    // Modifiers
    void clear() noexcept {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.clear();
    }
    
    template <typename P>
    std::pair<bool, T*> insert(P&& value) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_map.insert(std::forward<P>(value));
        return {result.second, result.first != m_map.end() ? &result.first->second : nullptr};
    }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.insert(first, last);
    }
    
    template <class K, class M>
    std::pair<bool, T*> insert_or_assign(K&& key, M&& value) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_map.insert_or_assign(std::forward<K>(key), std::forward<M>(value));
        return {result.second, &result.first->second};
    }
    
    template <class... Args>
    std::pair<bool, T*> emplace(Args&&... args) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_map.emplace(std::forward<Args>(args)...);
        return {result.second, result.first != m_map.end() ? &result.first->second : nullptr};
    }
    
    template <class K, class... Args>
    std::pair<bool, T*> try_emplace(K&& key, Args&&... value_args) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_map.try_emplace(std::forward<K>(key), std::forward<Args>(value_args)...);
        return {result.second, &result.first->second};
    }
    
    template <class K>
    size_type erase(const K& key) {
        std::unique_lock<Mutex> lock(m_mutex);
        return m_map.erase(key);
    }
    
    void swap(ordered_map_thread_safe& other) {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::unique_lock<Mutex> lock_other(other.m_mutex);
            m_map.swap(other.m_map);
        }
    }
    
    // Lookup
    template <class K>
    T& at(const K& key) {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.at(key);
    }
    
    template <class K>
    const T& at(const K& key) const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.at(key);
    }
    
    template <class K>
    T& operator[](K&& key) {
        std::unique_lock<Mutex> lock(m_mutex);
        return m_map[std::forward<K>(key)];
    }
    
    template <class K>
    size_type count(const K& key) const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.count(key);
    }
    
    template <class K>
    bool contains(const K& key) const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.contains(key);
    }
    
    // Hash policy
    float load_factor() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.load_factor();
    }
    
    float max_load_factor() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.max_load_factor();
    }
    
    void max_load_factor(float ml) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.max_load_factor(ml);
    }
    
    void rehash(size_type count) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.rehash(count);
    }
    
    void reserve(size_type count) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.reserve(count);
    }
    
    // Observers
    hasher hash_function() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.hash_function();
    }
    
    key_equal key_eq() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_map.key_eq();
    }
    
    // Serialization
    template <class Serializer>
    void serialize(Serializer& serializer) const {
        std::shared_lock<Mutex> lock(m_mutex);
        m_map.serialize(serializer);
    }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.deserialize(deserializer);
    }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer, bool hash_compatible) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_map.deserialize(deserializer, hash_compatible);
    }
    
private:
    ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> m_map;
    mutable Mutex m_mutex;
};

/**
 * Thread-safe wrapper for ordered_set using a shared_mutex (C++17).
 *
 * This wrapper allows multiple threads to read concurrently while
 * only one thread can write at a time. It provides the same interface
 * as ordered_set, but with lock guards around each operation.
 */
template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key, Allocator>,
          class IndexType = std::uint_least32_t,
          class Mutex = std::shared_mutex>
class ordered_set_thread_safe {
public:
    // Types
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    
    // Iterators are not exposed as they would break thread safety
    
    // Constructors
    ordered_set_thread_safe() = default;
    
    explicit ordered_set_thread_safe(size_type bucket_count,
                                    const Hash& hash = Hash(),
                                    const KeyEqual& equal = KeyEqual(),
                                    const Allocator& alloc = Allocator())
        : m_set(bucket_count, hash, equal, alloc) {}
    
    ordered_set_thread_safe(size_type bucket_count, const Allocator& alloc)
        : m_set(bucket_count, alloc) {}
    
    ordered_set_thread_safe(size_type bucket_count, const Hash& hash,
                           const Allocator& alloc)
        : m_set(bucket_count, hash, alloc) {}
    
    explicit ordered_set_thread_safe(const Allocator& alloc)
        : m_set(alloc) {}
    
    template <class InputIt>
    ordered_set_thread_safe(InputIt first, InputIt last,
                           size_type bucket_count = ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual(),
                           const Allocator& alloc = Allocator())
        : m_set(first, last, bucket_count, hash, equal, alloc) {}
    
    template <class InputIt>
    ordered_set_thread_safe(InputIt first, InputIt last, size_type bucket_count,
                           const Allocator& alloc)
        : m_set(first, last, bucket_count, alloc) {}
    
    template <class InputIt>
    ordered_set_thread_safe(InputIt first, InputIt last, size_type bucket_count,
                           const Hash& hash, const Allocator& alloc)
        : m_set(first, last, bucket_count, hash, alloc) {}
    
    ordered_set_thread_safe(std::initializer_list<value_type> init,
                           size_type bucket_count = ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual(),
                           const Allocator& alloc = Allocator())
        : m_set(init, bucket_count, hash, equal, alloc) {}
    
    ordered_set_thread_safe(std::initializer_list<value_type> init, size_type bucket_count,
                           const Allocator& alloc)
        : m_set(init, bucket_count, alloc) {}
    
    ordered_set_thread_safe(std::initializer_list<value_type> init, size_type bucket_count,
                           const Hash& hash, const Allocator& alloc)
        : m_set(init, bucket_count, hash, alloc) {}
    
    // Assignment operators
    ordered_set_thread_safe& operator=(std::initializer_list<value_type> ilist) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set = ilist;
        return *this;
    }
    
    ordered_set_thread_safe& operator=(const ordered_set_thread_safe& other) {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::shared_lock<Mutex> lock_other(other.m_mutex);
            m_set = other.m_set;
        }
        return *this;
    }
    
    ordered_set_thread_safe& operator=(ordered_set_thread_safe&& other) noexcept {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::unique_lock<Mutex> lock_other(other.m_mutex);
            m_set = std::move(other.m_set);
        }
        return *this;
    }
    
    // Capacity
    bool empty() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.empty();
    }
    
    size_type size() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.size();
    }
    
    size_type max_size() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.max_size();
    }
    
    // Modifiers
    void clear() noexcept {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.clear();
    }
    
    template <typename P>
    std::pair<bool, const Key*> insert(P&& value) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_set.insert(std::forward<P>(value));
        return {result.second, result.first != m_set.end() ? &(*result.first) : nullptr};
    }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.insert(first, last);
    }
    
    template <class... Args>
    std::pair<bool, const Key*> emplace(Args&&... args) {
        std::unique_lock<Mutex> lock(m_mutex);
        auto result = m_set.emplace(std::forward<Args>(args)...);
        return {result.second, result.first != m_set.end() ? &(*result.first) : nullptr};
    }
    
    template <class K>
    size_type erase(const K& key) {
        std::unique_lock<Mutex> lock(m_mutex);
        return m_set.erase(key);
    }
    
    void swap(ordered_set_thread_safe& other) {
        if (this != &other) {
            std::unique_lock<Mutex> lock_this(m_mutex);
            std::unique_lock<Mutex> lock_other(other.m_mutex);
            m_set.swap(other.m_set);
        }
    }
    
    // Lookup
    template <class K>
    size_type count(const K& key) const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.count(key);
    }
    
    template <class K>
    bool contains(const K& key) const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.contains(key);
    }
    
    // Hash policy
    float load_factor() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.load_factor();
    }
    
    float max_load_factor() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.max_load_factor();
    }
    
    void max_load_factor(float ml) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.max_load_factor(ml);
    }
    
    void rehash(size_type count) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.rehash(count);
    }
    
    void reserve(size_type count) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.reserve(count);
    }
    
    // Observers
    hasher hash_function() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.hash_function();
    }
    
    key_equal key_eq() const {
        std::shared_lock<Mutex> lock(m_mutex);
        return m_set.key_eq();
    }
    
    // Serialization
    template <class Serializer>
    void serialize(Serializer& serializer) const {
        std::shared_lock<Mutex> lock(m_mutex);
        m_set.serialize(serializer);
    }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.deserialize(deserializer);
    }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer, bool hash_compatible) {
        std::unique_lock<Mutex> lock(m_mutex);
        m_set.deserialize(deserializer, hash_compatible);
    }
    
private:
    ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> m_set;
    mutable Mutex m_mutex;
};

} // end namespace tsl

#endif