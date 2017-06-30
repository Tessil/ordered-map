/**
 * MIT License
 * 
 * Copyright (c) 2017 Tessil
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef TSL_ORDERED_MAP_H
#define TSL_ORDERED_MAP_H


#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if (defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ < 9))
#define TSL_NO_CONTAINER_ERASE_CONST_ITERATOR
#endif


/*
 * Only activate tsl_assert if TSL_DEBUG is defined. 
 * This way we avoid the performance hit when NDEBUG is not defined with assert as tsl_assert is used a lot
 * (people usually compile with "-O3" and not "-O3 -DNDEBUG").
 */
#ifndef tsl_assert
    #ifdef TSL_DEBUG
    #define tsl_assert(expr) assert(expr)
    #else
    #define tsl_assert(expr) (static_cast<void>(0))
    #endif
#endif

namespace tsl {

    
namespace detail_ordered_hash {
    
template<typename T>
struct make_void {
    using type = void;
};

template <typename T, typename = void>
struct has_is_transparent : std::false_type {
};

template <typename T>
struct has_is_transparent<T, typename make_void<typename T::is_transparent>::type> : std::true_type {
};


template <typename T, typename = void>
struct is_vector : std::false_type {
};

template <typename T>
struct is_vector<T, typename std::enable_if<
                        std::is_same<T, std::vector<typename T::value_type, typename T::allocator_type>>::value
                    >::type> : std::true_type {
};


/**
 * Each bucket entry stores a 32-bits index which is the index in m_values corresponding to the bucket's value 
 * and a 32 bits hash (truncated if the original was 64-bits) corresponding to the hash of the value.
 */
class bucket_entry {
public:
    using index_type = std::uint32_t;
    using truncated_hash_type = std::uint32_t;
    
    
    bucket_entry() noexcept : m_index(EMPTY_INDEX), m_hash(0) {
    }
    
    bool empty() const noexcept {
        return m_index == EMPTY_INDEX;
    }
    
    void clear() noexcept {
        m_index = EMPTY_INDEX;
    }
    
    index_type index() const noexcept {
        tsl_assert(!empty());
        return m_index;
    }
    
    void set_index(std::size_t index) noexcept {
        tsl_assert(index <= max_size());
        
        m_index = static_cast<index_type>(index);
    }
    
    truncated_hash_type truncated_hash() const noexcept {
        tsl_assert(!empty());
        return m_hash;
    }
    
    void set_hash(std::size_t hash) noexcept {
        m_hash = truncate_hash(hash);
    }
    
    
    
    static truncated_hash_type truncate_hash(std::size_t hash) noexcept {
        return static_cast<truncated_hash_type>(hash);
    }
    
    static std::size_t max_size() noexcept {
        return std::numeric_limits<index_type>::max() - NB_RESERVED_INDEXES;
    }
    
private:
    static const index_type EMPTY_INDEX = std::numeric_limits<index_type>::max();
    static const std::size_t NB_RESERVED_INDEXES = 1;
    
    index_type m_index;
    truncated_hash_type m_hash;
};

/**
 * Internal common class used by ordered_map and ordered_set.
 * 
 * ValueType is what will be stored by ordered_hash (usually std::pair<Key, T> for map and Key for set).
 * 
 * KeySelect should be a FunctionObject which takes a ValueType in parameter and return a reference to the key.
 * 
 * ValueSelect should be a FunctionObject which takes a ValueType in parameter and return a reference to the value. 
 * ValueSelect should be void if there is no value (in set for example).
 * 
 * ValueTypeContainer is the container which will be used to store ValueType values. 
 * Usually a std::deque<ValueType, Allocator> or std::vector<ValueType, Allocator>.
 */
template<class ValueType,
         class KeySelect,
         class ValueSelect,
         class Hash,
         class KeyEqual,
         class Allocator,
         class ValueTypeContainer>
class ordered_hash {
private:
    static_assert(std::is_same<typename ValueTypeContainer::value_type, ValueType>::value, 
                  "ValueTypeContainer::value_type != ValueType.");
    static_assert(std::is_same<typename ValueTypeContainer::allocator_type, Allocator>::value, 
                  "ValueTypeContainer::allocator_type != Allocator.");
    
    
public:
    template<bool is_const>
    class ordered_iterator;
    
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = ordered_iterator<false>;
    using const_iterator = ordered_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    using values_container_type = ValueTypeContainer;
    
public:
    template<bool is_const>
    class ordered_iterator {
        friend class ordered_hash;
        
    private:
        using iterator = typename std::conditional<is_const, 
                                                    typename values_container_type::const_iterator, 
                                                    typename values_container_type::iterator>::type;
    
        
        ordered_iterator(iterator it) noexcept : m_iterator(it) {
        }
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const typename ordered_hash::value_type;
        using difference_type = typename iterator::difference_type;
        using reference = value_type&;
        using pointer = value_type*;
        
        
        ordered_iterator() noexcept {
        }
        
        ordered_iterator(const ordered_iterator<false>& other) noexcept : m_iterator(other.m_iterator) {
        }

        const typename ordered_hash::key_type& key() const {
            return KeySelect()(*m_iterator);
        }

        template<class U = ValueSelect, typename std::enable_if<!std::is_same<U, void>::value>::type* = nullptr>
        typename std::conditional<is_const, const typename U::value_type&, typename U::value_type&>::type value() const
        {
            return m_iterator->second;
        }
        
        reference operator*() const { return *m_iterator; }
        pointer operator->() const { return m_iterator.operator->(); }
        
        ordered_iterator& operator++() { ++m_iterator; return *this; }
        ordered_iterator& operator--() { --m_iterator; return *this; }
        
        ordered_iterator operator++(int) { ordered_iterator tmp(*this); ++*this; return tmp; }
        ordered_iterator operator--(int) { ordered_iterator tmp(*this); --*this; return tmp; }
        
        reference operator[](difference_type n) const { return m_iterator[n]; }
        
        ordered_iterator& operator+=(difference_type n) { m_iterator += n; return *this; }
        ordered_iterator& operator-=(difference_type n) { m_iterator -= n; return *this; }
        
        ordered_iterator operator+(difference_type n) { ordered_iterator tmp(*this); tmp += n; return tmp; }
        ordered_iterator operator-(difference_type n) { ordered_iterator tmp(*this); tmp -= n; return tmp; }
        
        friend bool operator==(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator == rhs.m_iterator; 
        }
        
        friend bool operator!=(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator != rhs.m_iterator; 
        }
        
        friend bool operator<(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator < rhs.m_iterator; 
        }
        
        friend bool operator>(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator > rhs.m_iterator; 
        }
        
        friend bool operator<=(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator <= rhs.m_iterator; 
        }
        
        friend bool operator>=(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator >= rhs.m_iterator; 
        }

        friend ordered_iterator operator+(difference_type n, const ordered_iterator& it) { 
            return n + it.m_iterator;
        }

        friend difference_type operator-(const ordered_iterator& lhs, const ordered_iterator& rhs) { 
            return lhs.m_iterator - rhs.m_iterator; 
        }

    private:
        iterator m_iterator;
    };
    
    
private:
    using buckets_container_allocator = typename 
                            std::allocator_traits<allocator_type>::template rebind_alloc<bucket_entry>; 
                            
    using buckets_container_type = std::vector<bucket_entry, buckets_container_allocator>;
    
    
public:
    ordered_hash(size_type bucket_count, 
                 const Hash& hash,
                 const KeyEqual& equal,
                 const Allocator& alloc,
                 float max_load_factor) : m_buckets(alloc), m_values(alloc), m_hash(hash), m_key_equal(equal)
    {
        if(bucket_count == 0) {
            m_mask = 0;
        }
        else {
            m_buckets.resize(round_up_to_power_of_two(bucket_count));
            m_mask = this->bucket_count() - 1;
        }
        
        this->max_load_factor(max_load_factor);
    }
    
    allocator_type get_allocator() const {
        return m_values.get_allocator();
    }
    
    
    /*
     * Iterators
     */
    iterator begin() noexcept {
        return iterator(m_values.begin());
    }
    
    const_iterator begin() const noexcept {
        return cbegin();
    }
    
    const_iterator cbegin() const noexcept {
        return const_iterator(m_values.cbegin());
    }
    
    iterator end() noexcept {
        return iterator(m_values.end());
    }
    
    const_iterator end() const noexcept {
        return cend();
    }
    
    const_iterator cend() const noexcept {
        return const_iterator(m_values.cend());
    }  
    
    
    reverse_iterator rbegin() noexcept {
        return reverse_iterator(m_values.end());
    }
    
    const_reverse_iterator rbegin() const noexcept {
        return rcbegin();
    }
    
    const_reverse_iterator rcbegin() const noexcept {
        return const_reverse_iterator(m_values.cend());
    }
    
    reverse_iterator rend() noexcept {
        return reverse_iterator(m_values.begin());
    }
    
    const_reverse_iterator rend() const noexcept {
        return rcend();
    }
    
    const_reverse_iterator rcend() const noexcept {
        return const_reverse_iterator(m_values.cbegin());
    }  
    
    
    /*
     * Capacity
     */
    bool empty() const noexcept {
        return m_values.empty();
    }
    
    size_type size() const noexcept {
        return m_values.size();
    }
    
    size_type max_size() const noexcept {
        return std::min(bucket_entry::max_size(), std::min(m_values.max_size(), m_buckets.max_size()));
    }
    

    /*
     * Modifiers
     */
    void clear() noexcept {
        for(auto& bucket: m_buckets) {
            bucket.clear();
        }
        
        m_values.clear();
    }
    
    template<typename P>
    std::pair<iterator, bool> insert(P&& value) {
        return insert_impl(KeySelect()(value), m_hash(KeySelect()(value)), std::forward<P>(value));
    }
    
    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        if(std::is_base_of<std::forward_iterator_tag, 
                          typename std::iterator_traits<InputIt>::iterator_category>::value) 
        {
            const auto nb_elements_insert = std::distance(first, last);
            const std::size_t nb_free_buckets = bucket_count() - size();
            
            if(nb_elements_insert > 0 && nb_free_buckets < static_cast<std::size_t>(nb_elements_insert)) {
                reserve(size() + (nb_elements_insert - nb_free_buckets));
            }
        }
        
        for(; first != last; ++first) {
            insert(*first);
        }
    }
    
    
    template<class K, class M>
    std::pair<iterator, bool> insert_or_assign(K&& key, M&& value) {
        auto it = insert_impl(std::forward<K>(key), m_hash(key), std::forward<M>(value));
        if(!it.second) {
            it.first.value() = std::forward<M>(value);
        }
        
        return it;
    }
    
    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }
    
    template<class K, class... Args>
    std::pair<iterator, bool> try_emplace(K&& key, Args&&... value_args) {
        return insert_impl(std::forward<K>(key), m_hash(key), std::forward<Args>(value_args)...);
    }
    
    iterator erase(iterator pos) {
        return erase(const_iterator(pos));
    }
    
    iterator erase(const_iterator pos) {
        tsl_assert(pos != cend());
        
        const std::size_t index_erase = iterator_to_index(pos);
        
        auto it_bucket = find_key(pos.key(), m_hash(pos.key()));
        tsl_assert(it_bucket != m_buckets.end());
        
        erase_value_from_bucket(it_bucket);
        
        // One element was removed from m_values, 
        // due to the left shift the next element is now at the position of the previous element.
        return begin() + index_erase;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if(first == last) {
            return mutable_iterator(first);
        }
        
        tsl_assert(std::distance(first, last) > 0 && std::distance(cbegin(), first) >= 0);
        const std::size_t start_index = std::size_t(std::distance(cbegin(), first));
        const std::size_t nb_values = std::size_t(std::distance(first, last));
        const std::size_t end_index = start_index + nb_values;
        
        // Delete all values
#ifdef TSL_NO_CONTAINER_ERASE_CONST_ITERATOR     
        auto next_it = m_values.erase(mutable_iterator(first).m_iterator, mutable_iterator(last).m_iterator);   
#else
        auto next_it = m_values.erase(first.m_iterator, last.m_iterator);
#endif
        
        /*
         * TODO optimize
         * 
         * Mark the buckets corresponding to the values as empty and do a backward shift.
         * 
         * Also, the erase operation on m_values has shifted all the values on the right of last.m_iterator.
         * Adapt the indexes for these values.
         */
        for(std::size_t ibucket = 0; ibucket < m_buckets.size(); ibucket++) {
            if(m_buckets[ibucket].empty()) {
                continue;
            }
            
            if(m_buckets[ibucket].index() >= start_index && m_buckets[ibucket].index() < end_index) {
                m_buckets[ibucket].clear();
                backward_shift(ibucket);
            }
            else if(m_buckets[ibucket].index() >= end_index) {
                m_buckets[ibucket].set_index(m_buckets[ibucket].index() - nb_values);
            }
        }
        
        return iterator(next_it);
    }
    

    template<class K>
    size_type erase(const K& key) {
        return erase_impl(key);
    }
    
    void swap(ordered_hash& other) {
        using std::swap;
        
        swap(m_buckets, other.m_buckets);
        swap(m_values, other.m_values);
        swap(m_mask, other.m_mask);
        swap(m_max_load_factor, other.m_max_load_factor);
        swap(m_load_threshold, other.m_load_threshold);
        swap(m_hash, other.m_hash);
        swap(m_key_equal, other.m_key_equal);
    }
    
        
    

    /*
     * Lookup
     */    
    template<class K>
    size_type count(const K& key) const {
        if(find(key) == end()) {
            return 0;
        }
        else {
            return 1;
        }
    }
    
    template<class K>
    iterator find(const K& key) {
        if(empty()) {
            return end();
        }
        
        auto it_bucket = find_key(key, m_hash(key));
        return (it_bucket != m_buckets.end())?begin() + it_bucket->index():end();
    }
    
    template<class K>
    const_iterator find(const K& key) const {
        if(empty()) {
            return cend();
        }
        
        auto it_bucket = find_key(key, m_hash(key));
        return (it_bucket != m_buckets.cend())?cbegin() + it_bucket->index():cend();
    }
    
    template<class K>
    std::pair<iterator, iterator> equal_range(const K& key) {
        iterator it = find(key);
        return std::make_pair(it, it);
    }
    
    template<class K>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
        const_iterator it = find(key);
        return std::make_pair(it, it);
    }    
    
    
    /*
     * Bucket interface 
     */
    size_type bucket_count() const {
        return m_buckets.size(); 
    }
    
    size_type max_bucket_count() const {
        return m_buckets.max_size();
    }    
    
    /*
     *  Hash policy 
     */
    float load_factor() const {
        return float(size())/float(bucket_count());
    }
    
    float max_load_factor() const {
        return m_max_load_factor;
    }
    
    void max_load_factor(float ml) {
        m_max_load_factor = ml;
        m_load_threshold = size_type(float(bucket_count())*m_max_load_factor);
    }
    
    void rehash(size_type count) {
        count = std::max(count, size_type(std::ceil(float(size())/max_load_factor())));
        rehash_impl(count);
    }
    
    void reserve(size_type count) {
        count = size_type(std::ceil(float(count)/max_load_factor()));
        reserve_space_for_values(count);
        rehash(count);
    }
    
    
    /*
     * Observers
     */
    hasher hash_function() const {
        return m_hash;
    }
    
    key_equal key_eq() const {
        return m_key_equal;
    }    

    
    /*
     * Other
     */
    iterator mutable_iterator(const_iterator pos) {
        return begin() + iterator_to_index(pos);
    }
    
    template<class K, class U = ValueSelect, typename std::enable_if<!std::is_same<U, void>::value>::type* = nullptr>
    typename U::value_type& at(const K& key) {
        return const_cast<typename U::value_type&>(static_cast<const ordered_hash*>(this)->at(key));
    }
    
    template<class K, class U = ValueSelect, typename std::enable_if<!std::is_same<U, void>::value>::type* = nullptr>
    const typename U::value_type& at(const K& key) const {
        auto it = find(key);
        if(it != end()) {
            return it.value();
        }
        else {
            throw std::out_of_range("Couldn't find the key.");
        }
    }
    
    
    template<class K, class U = ValueSelect, typename std::enable_if<!std::is_same<U, void>::value>::type* = nullptr>
    typename U::value_type& operator[](K&& key) {
        // TODO Optimize
        using T = typename U::value_type;
        
        auto it = find(key);
        if(it != end()) {
            return it.value();
        }
        else {
            return insert(std::make_pair(std::forward<K>(key), T())).first.value();
        }
    }
    
    const_reference front() const {
        return m_values.front();
    }
    
    const_reference back() const {
        return m_values.back();
    }
    
    const values_container_type& values_container() const noexcept {
        return m_values;
    }
    
    template<class U = values_container_type, typename std::enable_if<is_vector<U>::value>::type* = nullptr>    
    const typename values_container_type::value_type* data() const noexcept {
        return m_values.data();
    }
    
    template<class U = values_container_type, typename std::enable_if<is_vector<U>::value>::type* = nullptr>    
    size_type capacity() const noexcept {
        return m_values.capacity();
    }
    
    void shrink_to_fit() {
        m_values.shrink_to_fit();
    }

    void pop_back() {
        if(empty()) {
            return;
        }
        
        erase(std::prev(end()));
    }
    
    
    iterator unordered_erase(iterator pos) {
        return unordered_erase(const_iterator(pos));
    }
    
    iterator unordered_erase(const_iterator pos) {
        const std::size_t index_erase = iterator_to_index(pos);
        unordered_erase(pos.key());
        
        // One element was deleted, index_erase now point to the next element
        return begin() + index_erase;
    }
    
    template<class K>
    size_type unordered_erase(const K& key) {
        if(empty()) {
            return 0;
        }
        
        auto it_bucket_key = find_key(key, m_hash(key));
        if(it_bucket_key == m_buckets.end()) {
            return 0;
        }
        

        auto it_bucket_last_elem = find_key(KeySelect()(back()), m_hash(KeySelect()(back())));
        tsl_assert(it_bucket_last_elem != m_buckets.end());
        tsl_assert(!it_bucket_last_elem->empty());
        tsl_assert(it_bucket_last_elem->index() == m_values.size() - 1);
        
        if(it_bucket_key != it_bucket_last_elem) {
            using std::swap;
            swap(m_values[it_bucket_key->index()], m_values[it_bucket_last_elem->index()]);
            
            const std::size_t tmp_index = it_bucket_key->index();
            it_bucket_key->set_index(it_bucket_last_elem->index());
            it_bucket_last_elem->set_index(tmp_index);
        }
        
        erase_value_from_bucket(it_bucket_key);
        
        return 1;
    }
    
    friend bool operator==(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values == rhs.m_values;
    }
    
    friend bool operator!=(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values != rhs.m_values;
    }
    
    friend bool operator<(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values < rhs.m_values;
    }
    
    friend bool operator<=(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values <= rhs.m_values;
    }
    
    friend bool operator>(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values > rhs.m_values;
    }
    
    friend bool operator>=(const ordered_hash& lhs, const ordered_hash& rhs) {
        return lhs.m_values >= rhs.m_values;
    }
    
    
private:
    template<class K>
    typename buckets_container_type::iterator find_key(const K& key, std::size_t hash) {
        auto it = static_cast<const ordered_hash*>(this)->find_key(key, hash);
        return m_buckets.begin() + std::distance(m_buckets.cbegin(), it);
    }
    
    /**
     * Return bucket which has the key 'key' or m_buckets.end() if none.
     */
    template<class K>
    typename buckets_container_type::const_iterator find_key(const K& key, std::size_t hash) const {
        const auto truncated_hash = bucket_entry::truncate_hash(hash);
        
        for(std::size_t ibucket = bucket_for_hash(hash), iprobe = 0; ; ibucket = next_probe(ibucket), ++iprobe) {
            if(m_buckets[ibucket].empty()) {
                return m_buckets.end();
            }
            else if(m_buckets[ibucket].truncated_hash() == truncated_hash && 
                    m_key_equal(key, KeySelect()(m_values[m_buckets[ibucket].index()]))) 
            {
                return m_buckets.begin() + ibucket;
            }
            else if(iprobe > dist_from_initial_bucket(ibucket)) {
                return m_buckets.end();
            }
        }
    }
    
    void rehash_impl(size_type count) {
        count = round_up_to_power_of_two(count);
        if(count > max_size()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        
        buckets_container_type old_buckets(count);
        m_buckets.swap(old_buckets);
        
        this->max_load_factor(m_max_load_factor);
        m_mask = this->bucket_count() - 1;
        
        
        for(const bucket_entry& old_bucket: old_buckets) {
            if(old_bucket.empty()) {
                continue;
            }
            
            const auto insert_hash = old_bucket.truncated_hash();
            const auto insert_index = old_bucket.index();
            
            for(std::size_t ibucket = bucket_for_hash(insert_hash), iprobe = 0; ; 
                ibucket = next_probe(ibucket), ++iprobe) 
            {
                if(m_buckets[ibucket].empty()) {
                    m_buckets[ibucket].set_index(insert_index);
                    m_buckets[ibucket].set_hash(insert_hash);
                    break;
                }
                
                const std::size_t distance = dist_from_initial_bucket(ibucket);
                if(iprobe > distance) {
                    insert_with_robin_hood_swap(next_probe(ibucket), distance + 1, 
                                                m_buckets[ibucket].index(), m_buckets[ibucket].truncated_hash());
                    
                    m_buckets[ibucket].set_index(insert_index);
                    m_buckets[ibucket].set_hash(insert_hash);
                    
                    break;
                }
            }
        }
    }
    
    template<class T = values_container_type, typename std::enable_if<is_vector<T>::value>::type* = nullptr>
    void reserve_space_for_values(size_type count) {
        m_values.reserve(count);
    }
    
    template<class T = values_container_type, typename std::enable_if<!is_vector<T>::value>::type* = nullptr>
    void reserve_space_for_values(size_type /*count*/) {
    }
    
    /**
     * Swap the empty bucket with the values on its right until we cross another empty bucket
     * or if the other bucket has a dist_from_initial_bucket == 0.
     */
    void backward_shift(std::size_t empty_ibucket) noexcept {
        tsl_assert(m_buckets[empty_ibucket].empty());
        
        std::size_t previous_ibucket = empty_ibucket;
        for(std::size_t current_ibucket = next_probe(previous_ibucket); 
            !m_buckets[current_ibucket].empty() && dist_from_initial_bucket(current_ibucket) > 0;
            previous_ibucket = current_ibucket, current_ibucket = next_probe(current_ibucket)) 
        {
            std::swap(m_buckets[current_ibucket], m_buckets[previous_ibucket]);
        }
    }
    
    void erase_value_from_bucket(typename buckets_container_type::iterator it_bucket) {
        tsl_assert(it_bucket != m_buckets.end() && !it_bucket->empty());
        
        m_values.erase(m_values.begin() + it_bucket->index());
        
        const std::size_t index_deleted = it_bucket->index();
        
        // m_values.erase shifted all the values on the right of the erased value, shift the indexes except if
        // it was the last value
        if(index_deleted != m_values.size()) {
            for(auto& bucket: m_buckets) {
                if(!bucket.empty() && bucket.index() > index_deleted) {
                    bucket.set_index(bucket.index() - 1);
                }
            }
        }
        
        // Mark the bucket as empty and do a backward shift of the values on the right
        it_bucket->clear();
        backward_shift(std::size_t(std::distance(m_buckets.begin(), it_bucket)));
    }
    
    template<class K>
    size_type erase_impl(const K& key) {
        if(empty()) {
            return 0;
        }
        
        auto it_bucket = find_key(key, m_hash(key));
        if(it_bucket != m_buckets.end()) {
            erase_value_from_bucket(it_bucket);
            
            return 1;
        }
        else {
            return 0;
        }
    }
    
    /**
     * From ibucket, search for an empty bucket to store the insert_index and the insert_hash.
     * 
     * If on the way we find a bucket with a value which is further away from its initial bucket
     * than our current probing, swap the indexes and the hashes and continue the search
     * for an empty bucket to store this new index and hash while continuing the swapping process.
     */
    void insert_with_robin_hood_swap(std::size_t ibucket, std::size_t iprobe, 
                                     typename bucket_entry::index_type insert_index, 
                                     typename bucket_entry::truncated_hash_type insert_hash) noexcept
    {
        while(true) {
            if(m_buckets[ibucket].empty()) {
                m_buckets[ibucket].set_index(insert_index);
                m_buckets[ibucket].set_hash(insert_hash);
                
                return;
            }
            
            const std::size_t distance = dist_from_initial_bucket(ibucket);
            if(iprobe > distance) {
                const auto tmp_index = m_buckets[ibucket].index();
                const auto tmp_hash = m_buckets[ibucket].truncated_hash();
                
                m_buckets[ibucket].set_index(insert_index);
                m_buckets[ibucket].set_hash(insert_hash);
                
                insert_index = tmp_index;
                insert_hash = tmp_hash;
                
                iprobe = distance;
            }
            
            ibucket = next_probe(ibucket);
            ++iprobe;
        }
    }
    
    std::size_t dist_from_initial_bucket(std::size_t ibucket) const noexcept {
        const std::size_t initial_bucket = bucket_for_hash(m_buckets[ibucket].truncated_hash());
        
        // If the bucket is smaller than the initial bucket for the value, there was a wrapping at the end of the 
        // bucket array due to the modulo.
        if(ibucket < initial_bucket) {
            return (bucket_count() + ibucket) - initial_bucket;
        }
        else {
            return ibucket - initial_bucket;
        }
    }

    template<class K, class P, typename std::enable_if<std::is_constructible<value_type, P&&>::value>::type* = nullptr>
    void emplace_back_values_container(K&& /*key*/, P&& value) {
        m_values.emplace_back(std::forward<P>(value));
    }
    
    template<class K, class... Args>
    void emplace_back_values_container(K&& key, Args&&... args) {
        m_values.emplace_back(value_type(std::piecewise_construct, 
                                    std::forward_as_tuple(std::forward<K>(key)), 
                                    std::forward_as_tuple(std::forward<Args>(args)...)));
    }
    
    template<class K, class... Args>
    std::pair<iterator, bool> insert_impl(K&& key, std::size_t hash, Args&&... value) {
        resize_if_needed(1);
        
        for(std::size_t ibucket = bucket_for_hash(hash), iprobe = 0; ; ibucket = next_probe(ibucket), ++iprobe) {
            if(m_buckets[ibucket].empty()) {
                emplace_back_values_container(std::forward<K>(key), std::forward<Args>(value)...);
                m_buckets[ibucket].set_index(m_values.size() - 1);
                m_buckets[ibucket].set_hash(hash);     
                
                return std::make_pair(std::prev(end()), true);
            }
            else if(m_buckets[ibucket].truncated_hash() == bucket_entry::truncate_hash(hash) && 
                    m_key_equal(key, KeySelect()(m_values[m_buckets[ibucket].index()]))) 
            {
                return std::make_pair(begin() + m_buckets[ibucket].index(), false);
            }
            else if(rehash_on_high_nb_probes(iprobe)) {
                return insert_impl(std::forward<K>(key), hash, std::forward<Args>(value)...);
            }
            else {
                const std::size_t distance = dist_from_initial_bucket(ibucket);
                if(iprobe > distance) {
                    emplace_back_values_container(std::forward<K>(key), std::forward<Args>(value)...);
                    
                    // Propagate the index and the hash of the current bucket to a more far away bucket
                    // Clear the current bucket so we can use it to insert the key. 
                    insert_with_robin_hood_swap(next_probe(ibucket), distance + 1, 
                                                m_buckets[ibucket].index(), m_buckets[ibucket].truncated_hash());
                        
                    m_buckets[ibucket].set_index(m_values.size() - 1);
                    m_buckets[ibucket].set_hash(hash);  
                    
                    return std::make_pair(std::prev(end()), true);
                }
            }
        }        
    }
    
    
    void resize_if_needed(std::size_t delta) {
        if(size() + delta >= m_load_threshold) {
            rehash_impl(m_buckets.size() * REHASH_SIZE_MULTIPLICATION_FACTOR);
        }
    }
    
    
    std::size_t next_probe(std::size_t index) const noexcept {
        tsl_assert(!m_buckets.empty());
        return (index + 1) & m_mask;
    }
    
    std::size_t bucket_for_hash(std::size_t hash) const noexcept {
        tsl_assert(!m_buckets.empty());
        return hash & m_mask;
    }    
    
    std::size_t iterator_to_index(const_iterator it) const noexcept {
        const auto dist = std::distance(cbegin(), it);
        tsl_assert(dist >= 0);
        
        return static_cast<std::size_t>(dist);
    }
    
    // TODO could be faster
    static std::size_t round_up_to_power_of_two(std::size_t value) noexcept {
        std::size_t power = 1;
        while(power < value) {
            power <<= 1;
        }
        
        return power;
    }
    
public:
    static const size_type DEFAULT_INIT_BUCKETS_SIZE = 16;
    static constexpr float DEFAULT_MAX_LOAD_FACTOR = 0.95f;
    static const size_type REHASH_SIZE_MULTIPLICATION_FACTOR = 2;
    
    
    static const size_type REHASH_ON_HIGH_NB_PROBES__NPROBES = 8;
    static constexpr float REHASH_ON_HIGH_NB_PROBES__MIN_LOAD_FACTOR = 0.5f;
    
    bool rehash_on_high_nb_probes(std::size_t nb_probes) {
        if(nb_probes >= REHASH_ON_HIGH_NB_PROBES__NPROBES && 
           load_factor() >= REHASH_ON_HIGH_NB_PROBES__MIN_LOAD_FACTOR) 
        {
            rehash_impl(m_buckets.size() * REHASH_SIZE_MULTIPLICATION_FACTOR);
            return true;
        }
        else {
            return false;
        }
    }
    
private:
    buckets_container_type m_buckets;
    values_container_type m_values;
    
    std::size_t m_mask;
    
    float m_max_load_factor;
    size_type m_load_threshold;
    
    Hash m_hash;
    KeyEqual m_key_equal;
};


}


/**
 * Implementation of an hash map using open adressing with robin hood with backshift delete to resolve collision.
 * 
 * The particularity of the hash map is that it remembers the order in which the elements were added and
 * provide a way to access the structure which stores these values through the 'values_container()' method. 
 * The used container is defined by ValueTypeContainer, by default a std::deque is used (faster on rehash) but
 * a std::vector may be used. In this case the map provides a 'data()' method which give a direct access 
 * to the memory used to store the values (which can be usefull to communicate with C API's).
 * 
 * 
 * Iterators invalidation:
 *  - clear, operator=, reserve, rehash: always invalidate the iterators (also invalidate end()).
 *  - insert, emplace, emplace_hint, operator[]: when a std::vector is used as ValueTypeContainer 
 *                                               and if size() < capacity(), only end(). 
 *                                               Otherwise all the iterators are invalidated if an insert occurs.
 *  - erase: when a std::vector is used as ValueTypeContainer invalidate the iterator of the erased element 
 *           and all the ones after the erased element (including end()). 
 *           Otherwise all the iterators are invalidated if an erase occurs.
 */
template<class Key, 
         class T, 
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = std::allocator<std::pair<Key, T>>,
         class ValueTypeContainer = std::deque<std::pair<Key, T>, Allocator>>
class ordered_map {
private:
    template<typename U>
    using has_is_transparent = tsl::detail_ordered_hash::has_is_transparent<U>;
    
    class KeySelect {
    public:
        using key_type = Key;
        
        const key_type& operator()(const std::pair<Key, T>& key_value) const {
            return key_value.first;
        }
        
        key_type& operator()(std::pair<Key, T>& key_value) {
            return key_value.first;
        }
    };  
    
    class ValueSelect {
    public:
        using value_type = T;
        
        const value_type& operator()(const std::pair<Key, T>& key_value) const {
            return key_value.second;
        }
        
        value_type& operator()(std::pair<Key, T>& key_value) {
            return key_value.second;
        }
    };
    
    using ht = detail_ordered_hash::ordered_hash<std::pair<Key, T>, KeySelect, ValueSelect,
                                                 Hash, KeyEqual, Allocator, ValueTypeContainer>;
    
public:
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
    
    
    /*
     * Constructors
     */
    ordered_map() : ordered_map(ht::DEFAULT_INIT_BUCKETS_SIZE) {
    }
    
    explicit ordered_map(size_type bucket_count, 
                         const Hash& hash = Hash(),
                         const KeyEqual& equal = KeyEqual(),
                         const Allocator& alloc = Allocator()) : 
                         m_ht(bucket_count, hash, equal, alloc, ht::DEFAULT_MAX_LOAD_FACTOR)
    {
    }
    
    ordered_map(size_type bucket_count,
                const Allocator& alloc) : ordered_map(bucket_count, Hash(), KeyEqual(), alloc)
    {
    }
    
    ordered_map(size_type bucket_count,
                const Hash& hash,
                const Allocator& alloc) : ordered_map(bucket_count, hash, KeyEqual(), alloc)
    {
    }
    
    explicit ordered_map(const Allocator& alloc) : ordered_map(ht::DEFAULT_INIT_BUCKETS_SIZE, alloc) {
    }
    
    template<class InputIt>
    ordered_map(InputIt first, InputIt last,
                size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                const Hash& hash = Hash(),
                const KeyEqual& equal = KeyEqual(),
                const Allocator& alloc = Allocator()) : ordered_map(bucket_count, hash, equal, alloc)
    {
        insert(first, last);
    }
    
    template<class InputIt>
    ordered_map(InputIt first, InputIt last,
                size_type bucket_count,
                const Allocator& alloc) : ordered_map(first, last, bucket_count, Hash(), KeyEqual(), alloc)
    {
    }
    
    template<class InputIt>
    ordered_map(InputIt first, InputIt last,
                size_type bucket_count,
                const Hash& hash,
                const Allocator& alloc) : ordered_map(first, last, bucket_count, hash, KeyEqual(), alloc)
    {
    }

    ordered_map(std::initializer_list<value_type> init,
                size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                const Hash& hash = Hash(),
                const KeyEqual& equal = KeyEqual(),
                const Allocator& alloc = Allocator()) : 
                ordered_map(init.begin(), init.end(), bucket_count, hash, equal, alloc)
    {
    }

    ordered_map(std::initializer_list<value_type> init,
                size_type bucket_count,
                const Allocator& alloc) : 
                ordered_map(init.begin(), init.end(), bucket_count, Hash(), KeyEqual(), alloc)
    {
    }

    ordered_map(std::initializer_list<value_type> init,
                size_type bucket_count,
                const Hash& hash,
                const Allocator& alloc) : 
                ordered_map(init.begin(), init.end(), bucket_count, hash, KeyEqual(), alloc)
    {
    }

    
    ordered_map& operator=(std::initializer_list<value_type> ilist) {
        m_ht.clear();
        
        m_ht.reserve(ilist.size());
        m_ht.insert(ilist.begin(), ilist.end());
        
        return *this;
    }
    
    allocator_type get_allocator() const { return m_ht.get_allocator(); }
    

    
    /*
     * Iterators
     */
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
    
    
    /*
     * Capacity
     */
    bool empty() const noexcept { return m_ht.empty(); }
    size_type size() const noexcept { return m_ht.size(); }
    size_type max_size() const noexcept { return m_ht.max_size(); }
    
    /*
     * Modifiers
     */
    void clear() noexcept { m_ht.clear(); }
    
    
    
    std::pair<iterator, bool> insert(const value_type& value) { return m_ht.insert(value); }
        
    template<class P, typename std::enable_if<std::is_constructible<value_type, P&&>::value>::type* = nullptr>
    std::pair<iterator, bool> insert(P&& value) { return m_ht.emplace(std::forward<P>(value)); }
    
    std::pair<iterator, bool> insert(value_type&& value) { return m_ht.insert(std::move(value)); }
    
    
    iterator insert(const_iterator hint, const value_type& value) { 
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), KeySelect()(value))) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.insert(value).first; 
    }
        
    template<class P, typename std::enable_if<std::is_constructible<value_type, P&&>::value>::type* = nullptr>
    iterator insert(const_iterator hint, P&& value) {
        value_type val(std::forward<P>(value));
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), KeySelect()(val))) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.insert(std::move(val)).first;
    }
    
    iterator insert(const_iterator hint, value_type&& value) { 
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), KeySelect()(value))) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.insert(std::move(value)).first; 
    }
    
    
    template<class InputIt>
    void insert(InputIt first, InputIt last) { m_ht.insert(first, last); }
    void insert(std::initializer_list<value_type> ilist) { m_ht.insert(ilist.begin(), ilist.end()); }

    
    
    
    template<class M>
    std::pair<iterator, bool> insert_or_assign(const key_type& k, M&& obj) { 
        return m_ht.insert_or_assign(k, std::forward<M>(obj)); 
    }

    template<class M>
    std::pair<iterator, bool> insert_or_assign(key_type&& k, M&& obj) { 
        return m_ht.insert_or_assign(std::move(k), std::forward<M>(obj)); 
    }
    
    
    template<class M>
    iterator insert_or_assign(const_iterator hint, const key_type& k, M&& obj) {
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), k)) { 
            auto it = m_ht.mutable_iterator(hint); 
            it.value() = std::forward<M>(obj);
            
            return it;
        }
        
        return m_ht.insert_or_assign(k, std::forward<M>(obj)).first;
    }
    
    template<class M>
    iterator insert_or_assign(const_iterator hint, key_type&& k, M&& obj) {
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), k)) { 
            auto it = m_ht.mutable_iterator(hint); 
            it.value() = std::forward<M>(obj);
            
            return it;
        }
        
        return m_ht.insert_or_assign(std::move(k), std::forward<M>(obj)).first;
    }
    
    /**
     * Due to the way elements are stored, emplace will need to move or copy the key-value once.
     * The method is equivalent to insert(value_type(std::forward<Args>(args)...));
     * 
     * Mainly here for compatibility with the std::unordered_map interface.
     */
    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return m_ht.emplace(std::forward<Args>(args)...); }
    
    /**
     * Due to the way elements are stored, emplace_hint will need to move or copy the key-value once.
     * The method is equivalent to insert(hint, value_type(std::forward<Args>(args)...));
     * 
     * Mainly here for compatibility with the std::unordered_map interface.
     */
    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return insert(hint, value_type(std::forward<Args>(args)...));        
    }
    
    
    
    
    template<class... Args>
    std::pair<iterator, bool> try_emplace(const key_type& k, Args&&... args) { 
        return m_ht.try_emplace(k, std::forward<Args>(args)...);
    }
    
    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args) {
        return m_ht.try_emplace(std::move(k), std::forward<Args>(args)...);
    }
    
    template<class... Args>
    iterator try_emplace(const_iterator hint, const key_type& k, Args&&... args) {
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), k)) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.try_emplace(k, std::forward<Args>(args)...).first;
    }
    
    template<class... Args>
    iterator try_emplace(const_iterator hint, key_type&& k, Args&&... args) {
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), k)) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.try_emplace(std::move(k), std::forward<Args>(args)...).first;
    }
    
    
    

    /**
     * When erasing an element, the insert order will be preserved and no holes will be present in the container
     * returned by 'values_container()'. 
     * 
     * The method is in O(n), if the order is not important 'unordered_erase(...)' method is faster with an O(1)
     * average complexity.
     */
    iterator erase(iterator pos) { return m_ht.erase(pos); }
    
    /**
     * @copydoc erase(iterator pos)
     */
    iterator erase(const_iterator pos) { return m_ht.erase(pos); }
    
    /**
     * @copydoc erase(iterator pos)
     */    
    iterator erase(const_iterator first, const_iterator last) { return m_ht.erase(first, last); }
    
    /**
     * @copydoc erase(iterator pos)
     */    
    size_type erase(const key_type& key) { return m_ht.erase(key); }
    
    /**
     * @copydoc erase(iterator pos)
     * 
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    size_type erase(const K& key) { return m_ht.erase(key); }
    
    
    
    void swap(ordered_map& other) { other.m_ht.swap(m_ht); }
    
    /*
     * Lookup
     */
    T& at(const Key& key) { return m_ht.at(key); }
    const T& at(const Key& key) const { return m_ht.at(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    T& at(const K& key) { return m_ht.at(key); }
    
    /**
     * @copydoc at(const K& key)
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    const T& at(const K& key) const { return m_ht.at(key); }
    
    
    
    T& operator[](const Key& key) { return m_ht[key]; }    
    T& operator[](Key&& key) { return m_ht[std::move(key)]; }
    
    
    
    size_type count(const Key& key) const { return m_ht.count(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    size_type count(const K& key) const { return m_ht.count(key); }
    
    
    
    iterator find(const Key& key) { return m_ht.find(key); }
    const_iterator find(const Key& key) const { return m_ht.find(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    iterator find(const K& key) { return m_ht.find(key); }
    
    /**
     * @copydoc find(const K& key)
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    const_iterator find(const K& key) const { return m_ht.find(key); }
    
    
    
    std::pair<iterator, iterator> equal_range(const Key& key) { return m_ht.equal_range(key); }
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const { return m_ht.equal_range(key); }

    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    std::pair<iterator, iterator> equal_range(const K& key) { return m_ht.equal_range(key); }
    
    /**
     * @copydoc equal_range(const K& key)
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return m_ht.equal_range(key); }
    
    /*
     * Bucket interface 
     */
    size_type bucket_count() const { return m_ht.bucket_count(); }
    size_type max_bucket_count() const { return m_ht.max_bucket_count(); }
    
    
    /*
     * Hash policy 
     */
    float load_factor() const { return m_ht.load_factor(); }
    float max_load_factor() const { return m_ht.max_load_factor(); }
    void max_load_factor(float ml) { m_ht.max_load_factor(ml); }
    
    void rehash(size_type count) { m_ht.rehash(count); }
    void reserve(size_type count) { m_ht.reserve(count); }
    
    
    /*
     * Observers
     */
    hasher hash_function() const { return m_ht.hash_function(); }
    key_equal key_eq() const { return m_ht.key_eq(); }
    
    
    
    /*
     * Other
     */
    const_reference front() const { return m_ht.front(); }
    const_reference back() const { return m_ht.back(); }
    
    /**
     * Only available if ValueTypeContainer is a std::vector. Same as calling 'values_container().data()'.
     */
    template<class U = values_container_type, typename std::enable_if<tsl::detail_ordered_hash::is_vector<U>::value>::type* = nullptr>    
    const typename values_container_type::value_type* data() const noexcept { return m_ht.data(); }
        
    /**
     * Return the container in which the values are stored. The values are in the same order as the insertion order
     * and are contiguous in the structure, no holes (size() == values_container().size()).
     */
    const values_container_type& values_container() const noexcept { return m_ht.values_container(); }

    template<class U = values_container_type, typename std::enable_if<tsl::detail_ordered_hash::is_vector<U>::value>::type* = nullptr>    
    size_type capacity() const noexcept { return m_ht.capacity(); }
    
    void shrink_to_fit() { m_ht.shrink_to_fit(); }
    
    void pop_back() { m_ht.pop_back(); }
    
    /**
     * Faster erase operation with an O(1) average complexity but it doesn't preserve the insertion order.
     * 
     * If an erasure occurs, the last element of the map will take the place of the erased element.
     */
    iterator unordered_erase(iterator pos) { return m_ht.unordered_erase(pos); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     */
    iterator unordered_erase(const_iterator pos) { return m_ht.unordered_erase(pos); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     */    
    size_type unordered_erase(const key_type& key) { return m_ht.unordered_erase(key); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     * 
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    size_type unordered_erase(const K& key) { return m_ht.unordered_erase(key); }
    
    
    
    friend bool operator==(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht == rhs.m_ht; }
    friend bool operator!=(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht != rhs.m_ht; }
    friend bool operator<(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht < rhs.m_ht; }
    friend bool operator<=(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht <= rhs.m_ht; }
    friend bool operator>(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht > rhs.m_ht; }
    friend bool operator>=(const ordered_map& lhs, const ordered_map& rhs) { return lhs.m_ht >= rhs.m_ht; }
    
    friend void swap(ordered_map& lhs, ordered_map& rhs) { lhs.swap(rhs); }

private:
    ht m_ht;
};






/**
 * Implementation of an hash set using open adressing with robin hood with backshift delete to resolve collision.
 * 
 * The particularity of the hash set is that it remembers the order in which the elements were added and
 * provide a way to access the structure which stores these values through the 'values_container()' method. 
 * The used container is defined by ValueTypeContainer, by default a std::deque is used (faster on rehash) but
 * a std::vector may be used. In this case the set provides a 'data()' method which give a direct access 
 * to the memory used to store the values (which can be usefull to communicate with C API's).
 * 
 * 
 * Iterators invalidation:
 *  - clear, operator=, reserve, rehash: always invalidate the iterators (also invalidate end()).
 *  - insert, emplace, emplace_hint, operator[]: when a std::vector is used as ValueTypeContainer 
 *                                               and if size() < capacity(), only end(). 
 *                                               Otherwise all the iterators are invalidated if an insert occurs.
 *  - erase: when a std::vector is used as ValueTypeContainer invalidate the iterator of the erased element 
 *           and all the ones after the erased element (including end()). 
 *           Otherwise all the iterators are invalidated if an erase occurs.
 */
template<class Key, 
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = std::allocator<Key>,
         class ValueTypeContainer = std::deque<Key, Allocator>>
class ordered_set {
private:
    template<typename U>
    using has_is_transparent = tsl::detail_ordered_hash::has_is_transparent<U>;
    
    class KeySelect {
    public:
        using key_type = Key;
        
        const key_type& operator()(const Key& key) const {
            return key;
        }
        
        key_type& operator()(Key& key) {
            return key;
        }
    };
    
    using ht = detail_ordered_hash::ordered_hash<Key, KeySelect, void,
                                                 Hash, KeyEqual, Allocator, ValueTypeContainer>;
            
public:
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

    
    /*
     * Constructors
     */
    ordered_set() : ordered_set(ht::DEFAULT_INIT_BUCKETS_SIZE) {
    }
    
    explicit ordered_set(size_type bucket_count, 
                        const Hash& hash = Hash(),
                        const KeyEqual& equal = KeyEqual(),
                        const Allocator& alloc = Allocator()) : 
                        m_ht(bucket_count, hash, equal, alloc, ht::DEFAULT_MAX_LOAD_FACTOR)
    {
    }
    
    ordered_set(size_type bucket_count,
                  const Allocator& alloc) : ordered_set(bucket_count, Hash(), KeyEqual(), alloc)
    {
    }
    
    ordered_set(size_type bucket_count,
                  const Hash& hash,
                  const Allocator& alloc) : ordered_set(bucket_count, hash, KeyEqual(), alloc)
    {
    }
    
    explicit ordered_set(const Allocator& alloc) : ordered_set(ht::DEFAULT_INIT_BUCKETS_SIZE, alloc) {
    }
    
    template<class InputIt>
    ordered_set(InputIt first, InputIt last,
                size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                const Hash& hash = Hash(),
                const KeyEqual& equal = KeyEqual(),
                const Allocator& alloc = Allocator()) : ordered_set(bucket_count, hash, equal, alloc)
    {
        insert(first, last);
    }
    
    template<class InputIt>
    ordered_set(InputIt first, InputIt last,
                size_type bucket_count,
                const Allocator& alloc) : ordered_set(first, last, bucket_count, Hash(), KeyEqual(), alloc)
    {
    }
    
    template<class InputIt>
    ordered_set(InputIt first, InputIt last,
                size_type bucket_count,
                const Hash& hash,
                const Allocator& alloc) : ordered_set(first, last, bucket_count, hash, KeyEqual(), alloc)
    {
    }

    ordered_set(std::initializer_list<value_type> init,
                    size_type bucket_count = ht::DEFAULT_INIT_BUCKETS_SIZE,
                    const Hash& hash = Hash(),
                    const KeyEqual& equal = KeyEqual(),
                    const Allocator& alloc = Allocator()) : 
                    ordered_set(init.begin(), init.end(), bucket_count, hash, equal, alloc)
    {
    }

    ordered_set(std::initializer_list<value_type> init,
                    size_type bucket_count,
                    const Allocator& alloc) : 
                    ordered_set(init.begin(), init.end(), bucket_count, Hash(), KeyEqual(), alloc)
    {
    }

    ordered_set(std::initializer_list<value_type> init,
                    size_type bucket_count,
                    const Hash& hash,
                    const Allocator& alloc) : 
                    ordered_set(init.begin(), init.end(), bucket_count, hash, KeyEqual(), alloc)
    {
    }

    
    ordered_set& operator=(std::initializer_list<value_type> ilist) {
        m_ht.clear();
        
        m_ht.reserve(ilist.size());
        m_ht.insert(ilist.begin(), ilist.end());
        
        return *this;
    }
    
    allocator_type get_allocator() const { return m_ht.get_allocator(); }
    
    
    /*
     * Iterators
     */
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
    
    
    /*
     * Capacity
     */
    bool empty() const noexcept { return m_ht.empty(); }
    size_type size() const noexcept { return m_ht.size(); }
    size_type max_size() const noexcept { return m_ht.max_size(); }
    
    /*
     * Modifiers
     */
    void clear() noexcept { m_ht.clear(); }
    
    
    
    std::pair<iterator, bool> insert(const value_type& value) { return m_ht.insert(value); }
    std::pair<iterator, bool> insert(value_type&& value) { return m_ht.insert(std::move(value)); }
    
    iterator insert(const_iterator hint, const value_type& value) { 
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), KeySelect()(value))) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.insert(value).first; 
    }
    
    iterator insert(const_iterator hint, value_type&& value) { 
        if(hint != cend() && m_ht.key_eq()(KeySelect()(*hint), KeySelect()(value))) { 
            return m_ht.mutable_iterator(hint); 
        }
        
        return m_ht.insert(std::move(value)).first; 
    }
    
    template<class InputIt>
    void insert(InputIt first, InputIt last) { m_ht.insert(first, last); }
    void insert(std::initializer_list<value_type> ilist) { m_ht.insert(ilist.begin(), ilist.end()); }

    
    
    /**
     * Due to the way elements are stored, emplace will need to move or copy the key-value once.
     * The method is equivalent to insert(value_type(std::forward<Args>(args)...));
     * 
     * Mainly here for compatibility with the std::unordered_map interface.
     */
    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return m_ht.emplace(std::forward<Args>(args)...); }
    
    /**
     * Due to the way elements are stored, emplace_hint will need to move or copy the key-value once.
     * The method is equivalent to insert(hint, value_type(std::forward<Args>(args)...));
     * 
     * Mainly here for compatibility with the std::unordered_map interface.
     */
    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return m_ht.insert(hint, value_type(std::forward<Args>(args)...));              
    }

    /**
     * When erasing an element, the insert order will be preserved and no holes will be present in the container
     * returned by 'values_container()'. 
     * 
     * The method is in O(n), if the order is not important 'unordered_erase(...)' method is faster with an O(1)
     * average complexity.
     */    
    iterator erase(iterator pos) { return m_ht.erase(pos); }
    
    /**
     * @copydoc erase(iterator pos)
     */    
    iterator erase(const_iterator pos) { return m_ht.erase(pos); }
    
    /**
     * @copydoc erase(iterator pos)
     */    
    iterator erase(const_iterator first, const_iterator last) { return m_ht.erase(first, last); }
    
    /**
     * @copydoc erase(iterator pos)
     */    
    size_type erase(const key_type& key) { return m_ht.erase(key); }
    
    /**
     * @copydoc erase(iterator pos)
     * 
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    size_type erase(const K& key) { return m_ht.erase(key); }
    
    
    
    void swap(ordered_set& other) { other.m_ht.swap(m_ht); }
    
    /*
     * Lookup
     */
    size_type count(const Key& key) const { return m_ht.count(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>
    size_type count(const K& key) const { return m_ht.count(key); }
    
    
    
    
    iterator find(const Key& key) { return m_ht.find(key); }
    const_iterator find(const Key& key) const { return m_ht.find(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>
    iterator find(const K& key) { return m_ht.find(key); }
    
    /**
     * @copydoc find(const K& key)
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>
    const_iterator find(const K& key) const { return m_ht.find(key); }
    
    
    
    std::pair<iterator, iterator> equal_range(const Key& key) { return m_ht.equal_range(key); }
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const { return m_ht.equal_range(key); }
    
    /**
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    std::pair<iterator, iterator> equal_range(const K& key) { return m_ht.equal_range(key); }
    
    /**
     * @copydoc equal_range(const K& key)
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr>     
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return m_ht.equal_range(key); }
    

    /*
     * Bucket interface 
     */
    size_type bucket_count() const { return m_ht.bucket_count(); }
    size_type max_bucket_count() const { return m_ht.max_bucket_count(); }
    
    
    /*
     *  Hash policy 
     */
    float load_factor() const { return m_ht.load_factor(); }
    float max_load_factor() const { return m_ht.max_load_factor(); }
    void max_load_factor(float ml) { m_ht.max_load_factor(ml); }
    
    void rehash(size_type count) { m_ht.rehash(count); }
    void reserve(size_type count) { m_ht.reserve(count); }
    
    
    /*
     * Observers
     */
    hasher hash_function() const { return m_ht.hash_function(); }
    key_equal key_eq() const { return m_ht.key_eq(); }
    
    
    /*
     * Other
     */
    const_reference front() const { return m_ht.front(); }
    const_reference back() const { return m_ht.back(); }
    
    /**
     * Only available if ValueTypeContainer is a std::vector. Same as calling 'values_container().data()'.
     */ 
    template<class U = values_container_type, typename std::enable_if<tsl::detail_ordered_hash::is_vector<U>::value>::type* = nullptr>    
    const typename values_container_type::value_type* data() const noexcept { return m_ht.data(); }
    
    /**
     * Return the container in which the values are stored. The values are in the same order as the insertion order
     * and are contiguous in the structure, no holes (size() == values_container().size()).
     */        
    const values_container_type& values_container() const noexcept { return m_ht.values_container(); }

    template<class U = values_container_type, typename std::enable_if<tsl::detail_ordered_hash::is_vector<U>::value>::type* = nullptr>    
    size_type capacity() const noexcept { return m_ht.capacity(); }
    
    void shrink_to_fit() { m_ht.shrink_to_fit(); }
    
    void pop_back() { m_ht.pop_back(); }
    
    /**
     * Faster erase operation with an O(1) average complexity but it doesn't preserve the insertion order.
     * 
     * If an erasure occurs, the last element of the map will take the place of the erased element.
     */    
    iterator unordered_erase(iterator pos) { return m_ht.unordered_erase(pos); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     */    
    iterator unordered_erase(const_iterator pos) { return m_ht.unordered_erase(pos); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     */    
    size_type unordered_erase(const key_type& key) { return m_ht.unordered_erase(key); }
    
    /**
     * @copydoc unordered_erase(iterator pos)
     * 
     * This overload only participates in the overload resolution if the typedef KeyEqual::is_transparent exists. 
     * If so, K must be hashable and comparable to Key.
     */
    template<class K, class KE = KeyEqual, typename std::enable_if<has_is_transparent<KE>::value>::type* = nullptr> 
    size_type unordered_erase(const K& key) { return m_ht.unordered_erase(key); }
    
    
    
    friend bool operator==(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht == rhs.m_ht; }
    friend bool operator!=(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht != rhs.m_ht; }
    friend bool operator<(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht < rhs.m_ht; }
    friend bool operator<=(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht <= rhs.m_ht; }
    friend bool operator>(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht > rhs.m_ht; }
    friend bool operator>=(const ordered_set& lhs, const ordered_set& rhs) { return lhs.m_ht >= rhs.m_ht; }
    
    friend void swap(ordered_set& lhs, ordered_set& rhs) { lhs.swap(rhs); }
    
private:
    ht m_ht;    
};




}

#endif
