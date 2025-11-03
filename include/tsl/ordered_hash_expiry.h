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
#ifndef TSL_ORDERED_HASH_EXPIRY_H
#define TSL_ORDERED_HASH_EXPIRY_H

#include <chrono>
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

namespace detail_ordered_hash_expiry {

/**
 * Traits to determine if a type is a std::chrono::duration
 */
template <typename T>
struct is_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

/**
 * Base class for expiry policies
 */
template <class ValueType, class KeySelect>
class expiry_policy {
public:
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    
    virtual ~expiry_policy() = default;
    
    /**
     * Called when a new element is inserted
     */
    virtual void on_insert(const key_type& key, const value_type& value) = 0;
    
    /**
     * Called when an element is accessed
     */
    virtual void on_access(const key_type& key, const value_type& value) = 0;
    
    /**
     * Called when an element is erased
     */
    virtual void on_erase(const key_type& key) = 0;
    
    /**
     * Called when the hash table is cleared
     */
    virtual void on_clear() = 0;
    
    /**
     * Returns the next element that should expire, or nullptr if none
     */
    virtual const key_type* next_expired() const = 0;
    
    /**
     * Expires the next expired element and returns true if any was expired
     */
    virtual bool expire_next() = 0;
    
    /**
     * Expires all expired elements and returns the number of expired elements
     */
    virtual std::size_t expire_all() = 0;
};

/**
 * No expiry policy - default
 */
template <class ValueType, class KeySelect>
class no_expiry_policy : public expiry_policy<ValueType, KeySelect> {
public:
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    
    void on_insert(const key_type& /*key*/, const value_type& /*value*/) override {}
    void on_access(const key_type& /*key*/, const value_type& /*value*/) override {}
    void on_erase(const key_type& /*key*/) override {}
    void on_clear() override {}
    
    const key_type* next_expired() const override { return nullptr; }
    bool expire_next() override { return false; }
    std::size_t expire_all() override { return 0; }
};

/**
 * Time-based expiry policy (TTL)
 */
template <class ValueType, class KeySelect, class Clock = std::chrono::steady_clock>
class ttl_expiry_policy : public expiry_policy<ValueType, KeySelect> {
public:
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    using duration = std::chrono::steady_clock::duration;
    
    explicit ttl_expiry_policy(duration ttl) : m_ttl(ttl) {}
    
    void on_insert(const key_type& key, const value_type& /*value*/) override {
        const auto now = Clock::now();
        m_expiry_map[key] = now + m_ttl;
        m_expiry_queue.emplace(now + m_ttl, key);
    }
    
    void on_access(const key_type& key, const value_type& /*value*/) override {
        const auto now = Clock::now();
        const auto new_expiry = now + m_ttl;
        
        // Update expiry time in map
        m_expiry_map[key] = new_expiry;
        
        // Add to queue (we'll clean up old entries when checking expiry)
        m_expiry_queue.emplace(new_expiry, key);
    }
    
    void on_erase(const key_type& key) override {
        m_expiry_map.erase(key);
        // We don't erase from queue immediately - we'll clean it up when checking expiry
    }
    
    void on_clear() override {
        m_expiry_map.clear();
        m_expiry_queue = decltype(m_expiry_queue)();
    }
    
    const key_type* next_expired() const override {
        auto now = Clock::now();
        
        // Clean up expired entries and entries for keys that no longer exist
        while (!m_expiry_queue.empty()) {
            const auto& [expiry_time, key] = m_expiry_queue.top();
            
            // If this entry is not expired yet, we're done
            if (expiry_time > now) {
                break;
            }
            
            // Check if the key still exists in the map
            auto it = m_expiry_map.find(key);
            if (it == m_expiry_map.end() || it->second != expiry_time) {
                // This entry is stale, remove it from the queue
                const_cast<ttl_expiry_policy*>(this)->m_expiry_queue.pop();
                continue;
            }
            
            // Found an expired key that still exists
            return &key;
        }
        
        return nullptr;
    }
    
    bool expire_next() override {
        const key_type* expired_key = next_expired();
        if (expired_key != nullptr) {
            m_expiry_map.erase(*expired_key);
            m_expiry_queue.pop();
            return true;
        }
        return false;
    }
    
    std::size_t expire_all() override {
        std::size_t count = 0;
        while (expire_next()) {
            count++;
        }
        return count;
    }
    
private:
    duration m_ttl;
    
    // Map from key to expiry time
    std::unordered_map<key_type, typename Clock::time_point>
        m_expiry_map;
    
    // Priority queue to track next expiry time
    using expiry_entry = std::pair<typename Clock::time_point, key_type>;
    struct expiry_entry_compare {
        bool operator()(const expiry_entry& a, const expiry_entry& b) const {
            // Priority queue is a max-heap by default, so we need to reverse the order
            return a.first > b.first;
        }
    };
    std::priority_queue<expiry_entry, std::vector<expiry_entry>, expiry_entry_compare>
        m_expiry_queue;
};

/**
 * LRU (Least Recently Used) expiry policy
 */
template <class ValueType, class KeySelect>
class lru_expiry_policy : public expiry_policy<ValueType, KeySelect> {
public:
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    
    explicit lru_expiry_policy(std::size_t max_size) : m_max_size(max_size) {}
    
    void on_insert(const key_type& key, const value_type& /*value*/) override {
        // Add to front of the list
        m_lru_list.emplace_front(key);
        m_key_to_iter[key] = m_lru_list.begin();
        
        // If we've exceeded max size, remove the least recently used
        if (m_lru_list.size() > m_max_size) {
            m_key_to_iter.erase(m_lru_list.back());
            m_lru_list.pop_back();
        }
    }
    
    void on_access(const key_type& key, const value_type& /*value*/) override {
        // Move the key to the front of the list
        auto it = m_key_to_iter.find(key);
        if (it != m_key_to_iter.end()) {
            m_lru_list.erase(it->second);
            m_lru_list.emplace_front(key);
            it->second = m_lru_list.begin();
        }
    }
    
    void on_erase(const key_type& key) override {
        auto it = m_key_to_iter.find(key);
        if (it != m_key_to_iter.end()) {
            m_lru_list.erase(it->second);
            m_key_to_iter.erase(it);
        }
    }
    
    void on_clear() override {
        m_lru_list.clear();
        m_key_to_iter.clear();
    }
    
    const key_type* next_expired() const override {
        // LRU doesn't have "expired" elements in the same way as TTL
        // Instead, it has elements that would be removed if we insert more
        if (m_lru_list.size() <= m_max_size) {
            return nullptr;
        }
        
        // Return the least recently used key
        return &m_lru_list.back();
    }
    
    bool expire_next() override {
        if (m_lru_list.size() <= m_max_size) {
            return false;
        }
        
        // Remove the least recently used key
        m_key_to_iter.erase(m_lru_list.back());
        m_lru_list.pop_back();
        return true;
    }
    
    std::size_t expire_all() override {
        std::size_t count = 0;
        while (expire_next()) {
            count++;
        }
        return count;
    }
    
private:
    std::size_t m_max_size;
    
    // Doubly linked list to track access order
    std::list<key_type> m_lru_list;
    
    // Map from key to its position in the list
    std::unordered_map<key_type, typename std::list<key_type>::iterator>
        m_key_to_iter;
};

/**
 * Ordered hash with support for expiry policies
 */
template <class ValueType, class KeySelect, class ValueSelect, class Hash, class KeyEqual,
          class Allocator, class ValueTypeContainer, class IndexType,
          class ExpiryPolicy = no_expiry_policy<ValueType, KeySelect>>
class ordered_hash_expiry : public ordered_hash<ValueType, KeySelect, ValueSelect, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_hash<ValueType, KeySelect, ValueSelect, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
public:
    // Types
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
    using reverse_iterator = typename base::reverse_iterator;
    using const_reverse_iterator = typename base::const_reverse_iterator;
    using values_container_type = typename base::values_container_type;
    using expiry_policy_type = ExpiryPolicy;
    
    // Constructors
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_hash_expiry(size_type bucket_count, const Hash& hash, const KeyEqual& equal,
                        const Allocator& alloc, float max_load_factor, PolicyArgs&&... policy_args)
        : base(bucket_count, hash, equal, alloc, max_load_factor),
          m_expiry_policy(std::forward<PolicyArgs>(policy_args)...),
          m_auto_expire_on_access(true) {}
    
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_hash_expiry(size_type bucket_count = base::DEFAULT_INIT_BUCKETS_SIZE,
                        const Hash& hash = Hash(),
                        const KeyEqual& equal = KeyEqual(),
                        const Allocator& alloc = Allocator(),
                        PolicyArgs&&... policy_args)
        : ordered_hash_expiry(bucket_count, hash, equal, alloc, base::DEFAULT_MAX_LOAD_FACTOR,
                            std::forward<PolicyArgs>(policy_args)...) {}
    
    template <class InputIt, typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_hash_expiry(InputIt first, InputIt last,
                        size_type bucket_count = base::DEFAULT_INIT_BUCKETS_SIZE,
                        const Hash& hash = Hash(),
                        const KeyEqual& equal = KeyEqual(),
                        const Allocator& alloc = Allocator(),
                        PolicyArgs&&... policy_args)
        : ordered_hash_expiry(bucket_count, hash, equal, alloc, base::DEFAULT_MAX_LOAD_FACTOR,
                            std::forward<PolicyArgs>(policy_args)...) {
        insert(first, last);
    }
    
    ordered_hash_expiry(std::initializer_list<value_type> init,
                        size_type bucket_count = base::DEFAULT_INIT_BUCKETS_SIZE,
                        const Hash& hash = Hash(),
                        const KeyEqual& equal = KeyEqual(),
                        const Allocator& alloc = Allocator())
        : ordered_hash_expiry(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}
    
    // Modifiers
    template <typename P>
    std::pair<iterator, bool> insert(P&& value) {
        expire_stale_entries();
        
        auto result = base::insert(std::forward<P>(value));
        if (result.second) {
            m_expiry_policy.on_insert(KeySelect()(value), value);
        }
        
        return result;
    }
    
    template <typename P>
    iterator insert_hint(const_iterator hint, P&& value) {
        expire_stale_entries();
        
        auto result = base::insert_hint(hint, std::forward<P>(value));
        m_expiry_policy.on_insert(KeySelect()(value), value);
        
        return result;
    }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        expire_stale_entries();
        
        base::insert(first, last);
        
        // Update expiry policy for all inserted elements
        for (; first != last; ++first) {
            m_expiry_policy.on_insert(KeySelect()(*first), *first);
        }
    }
    
    template <class K, class M>
    std::pair<iterator, bool> insert_or_assign(K&& key, M&& value) {
        expire_stale_entries();
        
        auto result = base::insert_or_assign(std::forward<K>(key), std::forward<M>(value));
        m_expiry_policy.on_insert(key, result.first->second);
        
        return result;
    }
    
    template <class K, class M>
    iterator insert_or_assign(const_iterator hint, K&& key, M&& value) {
        expire_stale_entries();
        
        auto result = base::insert_or_assign(hint, std::forward<K>(key), std::forward<M>(value));
        m_expiry_policy.on_insert(key, result->second);
        
        return result;
    }
    
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        expire_stale_entries();
        
        auto result = base::emplace(std::forward<Args>(args)...);
        if (result.second) {
            m_expiry_policy.on_insert(KeySelect()(result.first->second), result.first->second);
        }
        
        return result;
    }
    
    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        expire_stale_entries();
        
        auto result = base::emplace_hint(hint, std::forward<Args>(args)...);
        m_expiry_policy.on_insert(KeySelect()(*result), *result);
        
        return result;
    }
    
    template <class K, class... Args>
    std::pair<iterator, bool> try_emplace(K&& key, Args&&... value_args) {
        expire_stale_entries();
        
        auto result = base::try_emplace(std::forward<K>(key), std::forward<Args>(value_args)...);
        if (result.second) {
            m_expiry_policy.on_insert(key, result.first->second);
        }
        
        return result;
    }
    
    template <class K, class... Args>
    iterator try_emplace_hint(const_iterator hint, K&& key, Args&&... args) {
        expire_stale_entries();
        
        auto result = base::try_emplace_hint(hint, std::forward<K>(key), std::forward<Args>(args)...);
        m_expiry_policy.on_insert(key, result->second);
        
        return result;
    }
    
    iterator erase(iterator pos) {
        m_expiry_policy.on_erase(KeySelect()(*pos));
        return base::erase(pos);
    }
    
    iterator erase(const_iterator pos) {
        m_expiry_policy.on_erase(KeySelect()(*pos));
        return base::erase(pos);
    }
    
    iterator erase(const_iterator first, const_iterator last) {
        for (auto it = first; it != last; ++it) {
            m_expiry_policy.on_erase(KeySelect()(*it));
        }
        return base::erase(first, last);
    }
    
    template <class K>
    size_type erase(const K& key) {
        size_type count = base::erase(key);
        if (count > 0) {
            m_expiry_policy.on_erase(key);
        }
        return count;
    }
    
    void clear() noexcept {
        base::clear();
        m_expiry_policy.on_clear();
    }
    
    // Lookup
    template <class K, class U = ValueSelect,
              typename std::enable_if<base::template has_mapped_type<U>::value>::type* = nullptr>
    typename U::value_type& at(const K& key) {
        expire_stale_entries();
        
        auto it = base::find(key);
        if (it == base::end()) {
            TSL_OH_THROW_OR_TERMINATE(std::out_of_range, "Couldn't find the key.");
        }
        
        if (m_auto_expire_on_access) {
            m_expiry_policy.on_access(key, *it);
        }
        
        return it.value();
    }
    
    template <class K>
    iterator find(const K& key) {
        expire_stale_entries();
        
        auto it = base::find(key);
        if (it != base::end() && m_auto_expire_on_access) {
            m_expiry_policy.on_access(key, *it);
        }
        
        return it;
    }
    
    template <class K>
    const_iterator find(const K& key) const {
        const_cast<ordered_hash_expiry*>(this)->expire_stale_entries();
        
        auto it = base::find(key);
        if (it != base::end() && m_auto_expire_on_access) {
            const_cast<expiry_policy_type&>(m_expiry_policy).on_access(key, *it);
        }
        
        return it;
    }
    
    template <class K>
    bool contains(const K& key) const {
        const_cast<ordered_hash_expiry*>(this)->expire_stale_entries();
        return base::contains(key);
    }
    
    // Expiry management
    void expire_stale_entries() {
        const key_type* expired_key;
        while ((expired_key = m_expiry_policy.next_expired()) != nullptr) {
            base::erase(*expired_key);
            m_expiry_policy.on_erase(*expired_key);
        }
    }
    
    std::size_t expire_all() {
        std::size_t count = 0;
        const key_type* expired_key;
        while ((expired_key = m_expiry_policy.next_expired()) != nullptr) {
            base::erase(*expired_key);
            m_expiry_policy.on_erase(*expired_key);
            count++;
        }
        return count;
    }
    
    void auto_expire_on_access(bool enabled) {
        m_auto_expire_on_access = enabled;
    }
    
    bool auto_expire_on_access() const {
        return m_auto_expire_on_access;
    }
    
    expiry_policy_type& expiry_policy() {
        return m_expiry_policy;
    }
    
    const expiry_policy_type& expiry_policy() const {
        return m_expiry_policy;
    }
    
private:
    ExpiryPolicy m_expiry_policy;
    bool m_auto_expire_on_access;
};

} // end namespace detail_ordered_hash_expiry

/**
 * Ordered map with support for expiry policies
 */
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<Key, T>, Allocator>,
          class IndexType = std::uint_least32_t,
          class ExpiryPolicy = detail_ordered_hash_expiry::no_expiry_policy<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>>>
class ordered_map_expiry {
private:
    class KeySelect {
    public:
        using key_type = Key;

        const key_type& operator()(const std::pair<Key, T>& key_value) const noexcept {
            return key_value.first;
        }

        key_type& operator()(std::pair<Key, T>& key_value) noexcept {
            return key_value.first;
        }
    };

    class ValueSelect {
    public:
        using value_type = T;

        const value_type& operator()(const std::pair<Key, T>& key_value) const noexcept {
            return key_value.second;
        }

        value_type& operator()(std::pair<Key, T>& key_value) noexcept {
            return key_value.second;
        }
    };

    using ht = detail_ordered_hash_expiry::ordered_hash_expiry<std::pair<Key, T>, KeySelect, ValueSelect, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType, ExpiryPolicy>;

public:
    // Types
    using key_type = typename ht::key_type;
    using mapped_type = T;
    using value_type = typename ht::value_type;
    using size_type = typename ht::size_type;
    using difference_type = typename ht::difference_type;
    using hasher = typename ht::hasher;
    using key_equal = typename ht::key_equal;
    using allocator_type = typename ht::allocator_type;
    using reference = typename ht::reference;
    using const_reference = typename ht::const_reference;
    using pointer = typename ht::pointer;
    using const_pointer = typename ht::const_pointer;
    using iterator = typename ht::iterator;
    using const_iterator = typename ht::const_iterator;
    using reverse_iterator = typename ht::reverse_iterator;
    using const_reverse_iterator = typename ht::const_reverse_iterator;
    using values_container_type = typename ht::values_container_type;
    using expiry_policy_type = typename ht::expiry_policy_type;
    
    // Constructors
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_map_expiry(size_type bucket_count, const Hash& hash, const KeyEqual& equal,
                       const Allocator& alloc, float max_load_factor, PolicyArgs&&... policy_args)
        : m_ht(bucket_count, hash, equal, alloc, max_load_factor, std::forward<PolicyArgs>(policy_args)...) {}
    
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_map_expiry(size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator(),
                       PolicyArgs&&... policy_args)
        : m_ht(bucket_count, hash, equal, alloc, ht::DEFAULT_MAX_LOAD_FACTOR,
               std::forward<PolicyArgs>(policy_args)...) {}
    
    template <class InputIt, typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_map_expiry(InputIt first, InputIt last,
                       size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator(),
                       PolicyArgs&&... policy_args)
        : m_ht(first, last, bucket_count, hash, equal, alloc, std::forward<PolicyArgs>(policy_args)...) {}
    
    ordered_map_expiry(std::initializer_list<value_type> init,
                       size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator())
        : m_ht(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}
    
    // Assignment operator
    ordered_map_expiry& operator=(std::initializer_list<value_type> ilist) {
        m_ht.clear();
        m_ht.reserve(ilist.size());
        m_ht.insert(ilist.begin(), ilist.end());
        return *this;
    }
    
    // Iterators
    iterator begin() noexcept { return m_ht.begin(); }
    const_iterator begin() const noexcept { return m_ht.begin(); }
    const_iterator cbegin() const noexcept { return m_ht.cbegin(); }
    
    iterator end() noexcept { return m_ht.end(); }
    const_iterator end() const noexcept { return m_ht.end(); }
    const_iterator cend() const noexcept { return m_ht.cend(); }
    
    reverse_iterator rbegin() noexcept { return m_ht.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return m_ht.rbegin(); }
    const_reverse_iterator rcbegin() const noexcept { return m_ht.rcbegin(); }
    
    reverse_iterator rend() noexcept { return m_ht.rend(); }
    const_reverse_iterator rend() const noexcept { return m_ht.rend(); }
    const_reverse_iterator rcend() const noexcept { return m_ht.rcend(); }
    
    // Capacity
    bool empty() const noexcept { return m_ht.empty(); }
    size_type size() const noexcept { return m_ht.size(); }
    size_type max_size() const noexcept { return m_ht.max_size(); }
    
    // Modifiers
    void clear() noexcept { m_ht.clear(); }
    
    template <typename P>
    std::pair<iterator, bool> insert(P&& value) { return m_ht.insert(std::forward<P>(value)); }
    
    template <typename P>
    iterator insert_hint(const_iterator hint, P&& value) { return m_ht.insert_hint(hint, std::forward<P>(value)); }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) { m_ht.insert(first, last); }
    
    template <class K, class M>
    std::pair<iterator, bool> insert_or_assign(K&& key, M&& value) { 
        return m_ht.insert_or_assign(std::forward<K>(key), std::forward<M>(value)); 
    }
    
    template <class K, class M>
    iterator insert_or_assign(const_iterator hint, K&& key, M&& value) { 
        return m_ht.insert_or_assign(hint, std::forward<K>(key), std::forward<M>(value)); 
    }
    
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return m_ht.emplace(std::forward<Args>(args)...); }
    
    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) { return m_ht.emplace_hint(hint, std::forward<Args>(args)...); }
    
    template <class K, class... Args>
    std::pair<iterator, bool> try_emplace(K&& key, Args&&... value_args) { 
        return m_ht.try_emplace(std::forward<K>(key), std::forward<Args>(value_args)...); 
    }
    
    template <class K, class... Args>
    iterator try_emplace_hint(const_iterator hint, K&& key, Args&&... args) { 
        return m_ht.try_emplace_hint(hint, std::forward<K>(key), std::forward<Args>(args)...); 
    }
    
    iterator erase(iterator pos) { return m_ht.erase(pos); }
    iterator erase(const_iterator pos) { return m_ht.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_ht.erase(first, last); }
    
    template <class K>
    size_type erase(const K& key) { return m_ht.erase(key); }
    
    void swap(ordered_map_expiry& other) { m_ht.swap(other.m_ht); }
    
    // Lookup
    template <class K>
    typename ValueSelect::value_type& at(const K& key) { return m_ht.at(key); }
    
    template <class K>
    const typename ValueSelect::value_type& at(const K& key) const { return m_ht.at(key); }
    
    template <class K>
    typename ValueSelect::value_type& operator[](K&& key) { return m_ht[std::forward<K>(key)]; }
    
    template <class K>
    size_type count(const K& key) const { return m_ht.count(key); }
    
    template <class K>
    iterator find(const K& key) { return m_ht.find(key); }
    
    template <class K>
    const_iterator find(const K& key) const { return m_ht.find(key); }
    
    template <class K>
    bool contains(const K& key) const { return m_ht.contains(key); }
    
    template <class K>
    std::pair<iterator, iterator> equal_range(const K& key) { return m_ht.equal_range(key); }
    
    template <class K>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return m_ht.equal_range(key); }
    
    // Bucket interface
    size_type bucket_count() const { return m_ht.bucket_count(); }
    size_type max_bucket_count() const { return m_ht.max_bucket_count(); }
    
    // Hash policy
    float load_factor() const { return m_ht.load_factor(); }
    float max_load_factor() const { return m_ht.max_load_factor(); }
    void max_load_factor(float ml) { m_ht.max_load_factor(ml); }
    void rehash(size_type count) { m_ht.rehash(count); }
    void reserve(size_type count) { m_ht.reserve(count); }
    
    // Observers
    hasher hash_function() const { return m_ht.hash_function(); }
    key_equal key_eq() const { return m_ht.key_eq(); }
    
    // Other
    iterator mutable_iterator(const_iterator pos) { return m_ht.mutable_iterator(pos); }
    iterator nth(size_type index) { return m_ht.nth(index); }
    const_iterator nth(size_type index) const { return m_ht.nth(index); }
    
    const_reference front() const { return m_ht.front(); }
    const_reference back() const { return m_ht.back(); }
    
    values_container_type& values_container() { return m_ht.values_container(); }
    const values_container_type& values_container() const { return m_ht.values_container(); }
    
    template <class T = ValueTypeContainer,
              typename std::enable_if<detail_ordered_hash::is_vector<T>::value>::type* = nullptr>
    pointer data() { return m_ht.data(); }
    
    template <class T = ValueTypeContainer,
              typename std::enable_if<detail_ordered_hash::is_vector<T>::value>::type* = nullptr>
    const_pointer data() const { return m_ht.data(); }
    
    // Expiry management
    void expire_stale_entries() { m_ht.expire_stale_entries(); }
    std::size_t expire_all() { return m_ht.expire_all(); }
    void auto_expire_on_access(bool enabled) { m_ht.auto_expire_on_access(enabled); }
    bool auto_expire_on_access() const { return m_ht.auto_expire_on_access(); }
    
    expiry_policy_type& expiry_policy() { return m_ht.expiry_policy(); }
    const expiry_policy_type& expiry_policy() const { return m_ht.expiry_policy(); }
    
    // Serialization
    template <class Serializer>
    void serialize(Serializer& serializer) const { m_ht.serialize(serializer); }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer) { m_ht.deserialize(deserializer); }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer, bool hash_compatible) { 
        m_ht.deserialize(deserializer, hash_compatible); 
    }
    
private:
    ht m_ht;
};

/**
 * Ordered set with support for expiry policies
 */
template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key, Allocator>,
          class IndexType = std::uint_least32_t,
          class ExpiryPolicy = detail_ordered_hash_expiry::no_expiry_policy<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>>>
class ordered_set_expiry {
private:
    class KeySelect {
    public:
        using key_type = Key;

        const key_type& operator()(const Key& key) const noexcept { return key; }

        key_type& operator()(Key& key) noexcept { return key; }
    };

    using ht = detail_ordered_hash_expiry::ordered_hash_expiry<Key, KeySelect, void, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType, ExpiryPolicy>;

public:
    // Types
    using key_type = typename ht::key_type;
    using value_type = typename ht::value_type;
    using size_type = typename ht::size_type;
    using difference_type = typename ht::difference_type;
    using hasher = typename ht::hasher;
    using key_equal = typename ht::key_equal;
    using allocator_type = typename ht::allocator_type;
    using reference = typename ht::reference;
    using const_reference = typename ht::const_reference;
    using pointer = typename ht::pointer;
    using const_pointer = typename ht::const_pointer;
    using iterator = typename ht::iterator;
    using const_iterator = typename ht::const_iterator;
    using reverse_iterator = typename ht::reverse_iterator;
    using const_reverse_iterator = typename ht::const_reverse_iterator;
    using values_container_type = typename ht::values_container_type;
    using expiry_policy_type = typename ht::expiry_policy_type;
    
    // Constructors
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_set_expiry(size_type bucket_count, const Hash& hash, const KeyEqual& equal,
                       const Allocator& alloc, float max_load_factor, PolicyArgs&&... policy_args)
        : m_ht(bucket_count, hash, equal, alloc, max_load_factor, std::forward<PolicyArgs>(policy_args)...) {}
    
    template <typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_set_expiry(size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator(),
                       PolicyArgs&&... policy_args)
        : m_ht(bucket_count, hash, equal, alloc, ht::DEFAULT_MAX_LOAD_FACTOR,
               std::forward<PolicyArgs>(policy_args)...) {}
    
    template <class InputIt, typename... PolicyArgs,
              typename std::enable_if<std::is_constructible<ExpiryPolicy, PolicyArgs...>::value>::type* = nullptr>
    ordered_set_expiry(InputIt first, InputIt last,
                       size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator(),
                       PolicyArgs&&... policy_args)
        : m_ht(first, last, bucket_count, hash, equal, alloc, std::forward<PolicyArgs>(policy_args)...) {}
    
    ordered_set_expiry(std::initializer_list<value_type> init,
                       size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const Allocator& alloc = Allocator())
        : m_ht(init.begin(), init.end(), bucket_count, hash, equal, alloc) {}
    
    // Assignment operator
    ordered_set_expiry& operator=(std::initializer_list<value_type> ilist) {
        m_ht.clear();
        m_ht.reserve(ilist.size());
        m_ht.insert(ilist.begin(), ilist.end());
        return *this;
    }
    
    // Iterators
    iterator begin() noexcept { return m_ht.begin(); }
    const_iterator begin() const noexcept { return m_ht.begin(); }
    const_iterator cbegin() const noexcept { return m_ht.cbegin(); }
    
    iterator end() noexcept { return m_ht.end(); }
    const_iterator end() const noexcept { return m_ht.end(); }
    const_iterator cend() const noexcept { return m_ht.cend(); }
    
    reverse_iterator rbegin() noexcept { return m_ht.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return m_ht.rbegin(); }
    const_reverse_iterator rcbegin() const noexcept { return m_ht.rcbegin(); }
    
    reverse_iterator rend() noexcept { return m_ht.rend(); }
    const_reverse_iterator rend() const noexcept { return m_ht.rend(); }
    const_reverse_iterator rcend() const noexcept { return m_ht.rcend(); }
    
    // Capacity
    bool empty() const noexcept { return m_ht.empty(); }
    size_type size() const noexcept { return m_ht.size(); }
    size_type max_size() const noexcept { return m_ht.max_size(); }
    
    // Modifiers
    void clear() noexcept { m_ht.clear(); }
    
    template <typename P>
    std::pair<iterator, bool> insert(P&& value) { return m_ht.insert(std::forward<P>(value)); }
    
    template <typename P>
    iterator insert_hint(const_iterator hint, P&& value) { return m_ht.insert_hint(hint, std::forward<P>(value)); }
    
    template <class InputIt>
    void insert(InputIt first, InputIt last) { m_ht.insert(first, last); }
    
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return m_ht.emplace(std::forward<Args>(args)...); }
    
    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) { return m_ht.emplace_hint(hint, std::forward<Args>(args)...); }
    
    iterator erase(iterator pos) { return m_ht.erase(pos); }
    iterator erase(const_iterator pos) { return m_ht.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_ht.erase(first, last); }
    
    template <class K>
    size_type erase(const K& key) { return m_ht.erase(key); }
    
    void swap(ordered_set_expiry& other) { m_ht.swap(other.m_ht); }
    
    // Lookup
    template <class K>
    size_type count(const K& key) const { return m_ht.count(key); }
    
    template <class K>
    iterator find(const K& key) { return m_ht.find(key); }
    
    template <class K>
    const_iterator find(const K& key) const { return m_ht.find(key); }
    
    template <class K>
    bool contains(const K& key) const { return m_ht.contains(key); }
    
    template <class K>
    std::pair<iterator, iterator> equal_range(const K& key) { return m_ht.equal_range(key); }
    
    template <class K>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return m_ht.equal_range(key); }
    
    // Bucket interface
    size_type bucket_count() const { return m_ht.bucket_count(); }
    size_type max_bucket_count() const { return m_ht.max_bucket_count(); }
    
    // Hash policy
    float load_factor() const { return m_ht.load_factor(); }
    float max_load_factor() const { return m_ht.max_load_factor(); }
    void max_load_factor(float ml) { m_ht.max_load_factor(ml); }
    void rehash(size_type count) { m_ht.rehash(count); }
    void reserve(size_type count) { m_ht.reserve(count); }
    
    // Observers
    hasher hash_function() const { return m_ht.hash_function(); }
    key_equal key_eq() const { return m_ht.key_eq(); }
    
    // Other
    iterator mutable_iterator(const_iterator pos) { return m_ht.mutable_iterator(pos); }
    iterator nth(size_type index) { return m_ht.nth(index); }
    const_iterator nth(size_type index) const { return m_ht.nth(index); }
    
    const_reference front() const { return m_ht.front(); }
    const_reference back() const { return m_ht.back(); }
    
    values_container_type& values_container() { return m_ht.values_container(); }
    const values_container_type& values_container() const { return m_ht.values_container(); }
    
    template <class T = ValueTypeContainer,
              typename std::enable_if<detail_ordered_hash::is_vector<T>::value>::type* = nullptr>
    pointer data() { return m_ht.data(); }
    
    template <class T = ValueTypeContainer,
              typename std::enable_if<detail_ordered_hash::is_vector<T>::value>::type* = nullptr>
    const_pointer data() const { return m_ht.data(); }
    
    // Expiry management
    void expire_stale_entries() { m_ht.expire_stale_entries(); }
    std::size_t expire_all() { return m_ht.expire_all(); }
    void auto_expire_on_access(bool enabled) { m_ht.auto_expire_on_access(enabled); }
    bool auto_expire_on_access() const { return m_ht.auto_expire_on_access(); }
    
    expiry_policy_type& expiry_policy() { return m_ht.expiry_policy(); }
    const expiry_policy_type& expiry_policy() const { return m_ht.expiry_policy(); }
    
    // Serialization
    template <class Serializer>
    void serialize(Serializer& serializer) const { m_ht.serialize(serializer); }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer) { m_ht.deserialize(deserializer); }
    
    template <class Deserializer>
    void deserialize(Deserializer& deserializer, bool hash_compatible) { 
        m_ht.deserialize(deserializer, hash_compatible); 
    }
    
private:
    ht m_ht;
};

// Helper functions to create maps/sets with TTL expiry
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<Key, T>, Allocator>,
          class IndexType = std::uint_least32_t,
          class Duration,
          typename std::enable_if<detail_ordered_hash_expiry::is_duration<Duration>::value>::type* = nullptr>
ordered_map_expiry<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                   detail_ordered_hash_expiry::ttl_expiry_policy<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>>>
    make_ordered_map_with_ttl(Duration ttl,
                              size_type bucket_count = detail_ordered_hash::ordered_hash<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>, detail_ordered_hash::ordered_hash_default_value_select<std::pair<Key, T>>, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                              const Hash& hash = Hash(),
                              const KeyEqual& equal = KeyEqual(),
                              const Allocator& alloc = Allocator()) {
    return ordered_map_expiry<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                              detail_ordered_hash_expiry::ttl_expiry_policy<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>>>
        (bucket_count, hash, equal, alloc, ttl);
}

template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key, Allocator>,
          class IndexType = std::uint_least32_t,
          class Duration,
          typename std::enable_if<detail_ordered_hash_expiry::is_duration<Duration>::value>::type* = nullptr>
ordered_set_expiry<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                   detail_ordered_hash_expiry::ttl_expiry_policy<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>>>
    make_ordered_set_with_ttl(Duration ttl,
                              size_type bucket_count = detail_ordered_hash::ordered_hash<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>, void, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                              const Hash& hash = Hash(),
                              const KeyEqual& equal = KeyEqual(),
                              const Allocator& alloc = Allocator()) {
    return ordered_set_expiry<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                              detail_ordered_hash_expiry::ttl_expiry_policy<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>>>
        (bucket_count, hash, equal, alloc, ttl);
}

// Helper functions to create maps/sets with LRU expiry
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<Key, T>, Allocator>,
          class IndexType = std::uint_least32_t>
ordered_map_expiry<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                   detail_ordered_hash_expiry::lru_expiry_policy<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>>>
    make_ordered_map_with_lru(std::size_t max_size,
                              size_type bucket_count = detail_ordered_hash::ordered_hash<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>, detail_ordered_hash::ordered_hash_default_value_select<std::pair<Key, T>>, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                              const Hash& hash = Hash(),
                              const KeyEqual& equal = KeyEqual(),
                              const Allocator& alloc = Allocator()) {
    return ordered_map_expiry<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                              detail_ordered_hash_expiry::lru_expiry_policy<std::pair<Key, T>, detail_ordered_hash::ordered_hash_default_key_select<std::pair<Key, T>>>>
        (bucket_count, hash, equal, alloc, max_size);
}

template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key, Allocator>,
          class IndexType = std::uint_least32_t>
ordered_set_expiry<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                   detail_ordered_hash_expiry::lru_expiry_policy<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>>>
    make_ordered_set_with_lru(std::size_t max_size,
                              size_type bucket_count = detail_ordered_hash::ordered_hash<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>, void, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>::DEFAULT_INIT_BUCKETS_SIZE,
                              const Hash& hash = Hash(),
                              const KeyEqual& equal = KeyEqual(),
                              const Allocator& alloc = Allocator()) {
    return ordered_set_expiry<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType,
                              detail_ordered_hash_expiry::lru_expiry_policy<Key, detail_ordered_hash::ordered_hash_default_key_select<Key>>>
        (bucket_count, hash, equal, alloc, max_size);
}

} // end namespace tsl

#endif