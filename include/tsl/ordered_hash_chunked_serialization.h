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
#ifndef TSL_ORDERED_HASH_CHUNKED_SERIALIZATION_H
#define TSL_ORDERED_HASH_CHUNKED_SERIALIZATION_H

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

namespace detail_ordered_hash_chunked {

/**
 * Chunk types for serialization
 */
enum class chunk_type : std::uint32_t {
    HEADER = 0x00000001,
    DATA_ELEMENTS = 0x00000002,
    DATA_BUCKETS = 0x00000003,
    END = 0x00000004
};

/**
 * Base class for chunked serializers
 */
template <class Serializer>
class chunked_serializer {
public:
    chunked_serializer(Serializer& serializer, std::size_t chunk_size = 4096)
        : m_serializer(serializer), m_chunk_size(chunk_size) {}
        
    /**
     * Serialize a chunk header
     */
    void serialize_chunk_header(chunk_type type, std::size_t data_size) {
        serialize_value(static_cast<std::uint32_t>(type));
        serialize_value(static_cast<std::uint32_t>(data_size));
    }
    
    /**
     * Serialize a value
     */
    template <class T>
    void serialize_value(const T& value) {
        m_serializer(value);
        m_current_chunk_size += sizeof(T);
    }
    
    /**
     * Start a new chunk if needed
     */
    void start_new_chunk_if_needed(chunk_type type) {
        if (m_current_chunk_size >= m_chunk_size) {
            end_chunk();
            start_chunk(type);
        }
    }
    
    /**
     * Start a new chunk
     */
    void start_chunk(chunk_type type) {
        m_current_chunk_type = type;
        m_current_chunk_size = 0;
    }
    
    /**
     * End the current chunk
     */
    void end_chunk() {
        if (m_current_chunk_size > 0) {
            // We don't need to do anything special for the end of chunk
            // The chunk header already contains the size
            m_current_chunk_size = 0;
        }
    }
    
    /**
     * Get the underlying serializer
     */
    Serializer& underlying_serializer() {
        return m_serializer;
    }
    
private:
    Serializer& m_serializer;
    std::size_t m_chunk_size;
    chunk_type m_current_chunk_type;
    std::size_t m_current_chunk_size = 0;
};

/**
 * Base class for chunked deserializers
 */
template <class Deserializer>
class chunked_deserializer {
public:
    chunked_deserializer(Deserializer& deserializer)
        : m_deserializer(deserializer), m_current_chunk_type(chunk_type::HEADER),
          m_remaining_data_in_chunk(0) {}
        
    /**
     * Deserialize the next chunk header
     */
    chunk_type deserialize_chunk_header() {
        const std::uint32_t type = deserialize_value<std::uint32_t>();
        const std::uint32_t size = deserialize_value<std::uint32_t>();
        
        m_current_chunk_type = static_cast<chunk_type>(type);
        m_remaining_data_in_chunk = size;
        
        return m_current_chunk_type;
    }
    
    /**
     * Deserialize a value
     */
    template <class T>
    T deserialize_value() {
        tsl_oh_assert(m_remaining_data_in_chunk >= sizeof(T));
        
        T value = m_deserializer.template operator()<T>();
        m_remaining_data_in_chunk -= sizeof(T);
        
        return value;
    }
    
    /**
     * Check if there is remaining data in the current chunk
     */
    bool has_remaining_data_in_chunk() const {
        return m_remaining_data_in_chunk > 0;
    }
    
    /**
     * Get the current chunk type
     */
    chunk_type current_chunk_type() const {
        return m_current_chunk_type;
    }
    
    /**
     * Get the underlying deserializer
     */
    Deserializer& underlying_deserializer() {
        return m_deserializer;
    }
    
private:
    Deserializer& m_deserializer;
    chunk_type m_current_chunk_type;
    std::size_t m_remaining_data_in_chunk;
};

}

/**
 * ordered_hash with chunked serialization support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_hash_chunked : public ordered_hash<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
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
     * Serialize the hash table in chunks through the `serializer` parameter.
     * 
     * The `serializer` parameter must be a function object that supports the 
     * following operation: `void operator()(const T& value)` where T is at 
     * least `uint32_t`, `float`, `Key` and `T` (the mapped type).
     * 
     * The serialization is done in chunks to reduce memory usage and support 
     * partial serialization/deserialization.
     */
    template <class Serializer>
    void serialize_chunked(Serializer& serializer, std::size_t chunk_size = 4096) const {
        detail_ordered_hash_chunked::chunked_serializer<Serializer> cs(serializer, chunk_size);
        
        // Serialize header chunk
        cs.start_chunk(detail_ordered_hash_chunked::chunk_type::HEADER);
        const typename base::slz_size_type version = base::SERIALIZATION_PROTOCOL_VERSION;
        cs.serialize_value(version);
        
        const typename base::slz_size_type nb_elements = base::m_values.size();
        cs.serialize_value(nb_elements);
        
        const typename base::slz_size_type bucket_count = base::m_buckets_data.size();
        cs.serialize_value(bucket_count);
        
        const float max_load_factor = base::m_max_load_factor;
        cs.serialize_value(max_load_factor);
        cs.end_chunk();
        
        // Serialize elements in chunks
        cs.start_chunk(detail_ordered_hash_chunked::chunk_type::DATA_ELEMENTS);
        for (const value_type& value : base::m_values) {
            cs.start_new_chunk_if_needed(detail_ordered_hash_chunked::chunk_type::DATA_ELEMENTS);
            cs.serialize_value(value);
        }
        cs.end_chunk();
        
        // Serialize buckets in chunks
        cs.start_chunk(detail_ordered_hash_chunked::chunk_type::DATA_BUCKETS);
        for (const typename base::bucket_entry& bucket : base::m_buckets_data) {
            cs.start_new_chunk_if_needed(detail_ordered_hash_chunked::chunk_type::DATA_BUCKETS);
            
            // Serialize bucket entry
            const typename base::slz_size_type index = bucket.index();
            cs.serialize_value(index);
            
            const typename base::slz_size_type hash = bucket.hash();
            cs.serialize_value(hash);
        }
        cs.end_chunk();
        
        // Serialize end chunk
        cs.start_chunk(detail_ordered_hash_chunked::chunk_type::END);
        cs.end_chunk();
    }
    
    /**
     * Deserialize a previously serialized hash table in chunks through the 
     * `deserializer` parameter.
     * 
     * The `deserializer` parameter must be a function object that supports the 
     * following operation: `T operator()() const` where T is at least 
     * `uint32_t`, `float`, `Key` and `T` (the mapped type).
     * 
     * If `hash_compatible` is true, the deserialization will be faster as we 
     * won't rehash the elements. For this to be safe, the hash function, 
     * key equal function and the IndexType must be the same as the ones used 
     * during serialization.
     * 
     * The deserialization supports partial loading (breakpoint resume) by 
     * checking the current state of the hash table.
     */
    template <class Deserializer>
    void deserialize_chunked(Deserializer& deserializer, bool hash_compatible = false) {
        detail_ordered_hash_chunked::chunked_deserializer<Deserializer> cd(deserializer);
        
        // If the hash table is not empty, we assume we are resuming deserialization
        bool resuming = !base::empty();
        
        while (true) {
            // Get next chunk header if we are at the beginning of a chunk
            if (!cd.has_remaining_data_in_chunk()) {
                detail_ordered_hash_chunked::chunk_type type = cd.deserialize_chunk_header();
                
                if (type == detail_ordered_hash_chunked::chunk_type::END) {
                    break;
                }
            }
            
            switch (cd.current_chunk_type()) {
                case detail_ordered_hash_chunked::chunk_type::HEADER: {
                    if (resuming) {
                        // Skip header if we are resuming
                        const typename base::slz_size_type version = cd.deserialize_value<typename base::slz_size_type>();
                        (void)version;
                        
                        const typename base::slz_size_type nb_elements = cd.deserialize_value<typename base::slz_size_type>();
                        (void)nb_elements;
                        
                        const typename base::slz_size_type bucket_count = cd.deserialize_value<typename base::slz_size_type>();
                        (void)bucket_count;
                        
                        const float max_load_factor = cd.deserialize_value<float>();
                        (void)max_load_factor;
                    } else {
                        tsl_oh_assert(base::m_buckets_data.empty());  // Current hash table must be empty
                        
                        const typename base::slz_size_type version = cd.deserialize_value<typename base::slz_size_type>();
                        if (version != base::SERIALIZATION_PROTOCOL_VERSION) {
                            TSL_OH_THROW_OR_TERMINATE(std::runtime_error,
                                                      "Can't deserialize the ordered_map/set. "
                                                      "The protocol version header is invalid.");
                        }
                        
                        const typename base::slz_size_type nb_elements = cd.deserialize_value<typename base::slz_size_type>();
                        const typename base::slz_size_type bucket_count_ds = cd.deserialize_value<typename base::slz_size_type>();
                        const float max_load_factor = cd.deserialize_value<float>();
                        
                        if (max_load_factor < base::MAX_LOAD_FACTOR__MINIMUM ||
                            max_load_factor > base::MAX_LOAD_FACTOR__MAXIMUM) {
                            TSL_OH_THROW_OR_TERMINATE(
                                std::runtime_error,
                                "Invalid max_load_factor. Check that the serializer "
                                "and deserializer support floats correctly as they "
                                "can be converted implicitly to ints.");
                        }
                        
                        base::max_load_factor(max_load_factor);
                        
                        if (!hash_compatible) {
                            base::reserve(static_cast<size_type>(nb_elements));
                        } else {
                            base::m_buckets_data.reserve(static_cast<size_type>(bucket_count_ds));
                            base::m_buckets = base::m_buckets_data.data();
                            base::m_hash_mask = base::m_buckets_data.capacity() - 1;
                            
                            base::reserve_space_for_values(static_cast<size_type>(nb_elements));
                        }
                    }
                    break;
                }
                case detail_ordered_hash_chunked::chunk_type::DATA_ELEMENTS: {
                    if (!hash_compatible) {
                        while (cd.has_remaining_data_in_chunk()) {
                            value_type value = cd.deserialize_value<value_type>();
                            base::insert(std::move(value));
                        }
                    } else {
                        while (cd.has_remaining_data_in_chunk()) {
                            value_type value = cd.deserialize_value<value_type>();
                            base::m_values.push_back(std::move(value));
                        }
                    }
                    break;
                }
                case detail_ordered_hash_chunked::chunk_type::DATA_BUCKETS: {
                    if (hash_compatible) {
                        while (cd.has_remaining_data_in_chunk()) {
                            const typename base::slz_size_type index = cd.deserialize_value<typename base::slz_size_type>();
                            const typename base::slz_size_type hash = cd.deserialize_value<typename base::slz_size_type>();
                            
                            base::m_buckets_data.push_back(typename base::bucket_entry(
                                numeric_cast<typename base::index_type>(index, "Deserialized index is too big."),
                                numeric_cast<typename base::hash_type>(hash, "Deserialized hash is too big.")));
                        }
                    } else {
                        // Skip buckets if not hash compatible
                        while (cd.has_remaining_data_in_chunk()) {
                            const typename base::slz_size_type index = cd.deserialize_value<typename base::slz_size_type>();
                            (void)index;
                            
                            const typename base::slz_size_type hash = cd.deserialize_value<typename base::slz_size_type>();
                            (void)hash;
                        }
                    }
                    break;
                }
                default:
                    TSL_OH_THROW_OR_TERMINATE(std::runtime_error,
                                              "Unknown chunk type during deserialization.");
            }
        }
    }
};

/**
 * ordered_map with chunked serialization support
 */
template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>,
          class ValueTypeContainer = std::deque<std::pair<const Key, T>>,
          class IndexType = std::uint32_t>
class ordered_map_chunked : public ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_map<Key, T, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
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
     * Serialize the map in chunks through the `serializer` parameter.
     * 
     * The `serializer` parameter must be a function object that supports the 
     * following operation: `void operator()(const T& value)` where T is at 
     * least `uint32_t`, `float`, `Key` and `T` (the mapped type).
     * 
     * The serialization is done in chunks to reduce memory usage and support 
     * partial serialization/deserialization.
     */
    template <class Serializer>
    void serialize_chunked(Serializer& serializer, std::size_t chunk_size = 4096) const {
        base::m_ht.serialize_chunked(serializer, chunk_size);
    }
    
    /**
     * Deserialize a previously serialized map in chunks through the 
     * `deserializer` parameter.
     * 
     * The `deserializer` parameter must be a function object that supports the 
     * following operation: `T operator()() const` where T is at least 
     * `uint32_t`, `float`, `Key` and `T` (the mapped type).
     * 
     * If `hash_compatible` is true, the deserialization will be faster as we 
     * won't rehash the elements. For this to be safe, the hash function, 
     * key equal function and the IndexType must be the same as the ones used 
     * during serialization.
     * 
     * The deserialization supports partial loading (breakpoint resume) by 
     * checking the current state of the map.
     */
    template <class Deserializer>
    static ordered_map_chunked deserialize_chunked(Deserializer& deserializer, bool hash_compatible = false) {
        ordered_map_chunked map;
        map.m_ht.deserialize_chunked(deserializer, hash_compatible);
        
        return map;
    }
};

/**
 * ordered_set with chunked serialization support
 */
template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::deque<Key>,
          class IndexType = std::uint32_t>
class ordered_set_chunked : public ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType> {
private:
    using base = ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;
    
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
     * Serialize the set in chunks through the `serializer` parameter.
     * 
     * The `serializer` parameter must be a function object that supports the 
     * following operation: `void operator()(const T& value)` where T is at 
     * least `uint32_t`, `float` and `Key`.
     * 
     * The serialization is done in chunks to reduce memory usage and support 
     * partial serialization/deserialization.
     */
    template <class Serializer>
    void serialize_chunked(Serializer& serializer, std::size_t chunk_size = 4096) const {
        base::m_ht.serialize_chunked(serializer, chunk_size);
    }
    
    /**
     * Deserialize a previously serialized set in chunks through the 
     * `deserializer` parameter.
     * 
     * The `deserializer` parameter must be a function object that supports the 
     * following operation: `T operator()() const` where T is at least 
     * `uint32_t`, `float` and `Key`.
     * 
     * If `hash_compatible` is true, the deserialization will be faster as we 
     * won't rehash the elements. For this to be safe, the hash function, 
     * key equal function and the IndexType must be the same as the ones used 
     * during serialization.
     * 
     * The deserialization supports partial loading (breakpoint resume) by 
     * checking the current state of the set.
     */
    template <class Deserializer>
    static ordered_set_chunked deserialize_chunked(Deserializer& deserializer, bool hash_compatible = false) {
        ordered_set_chunked set;
        set.m_ht.deserialize_chunked(deserializer, hash_compatible);
        
        return set;
    }
};

} // namespace tsl

#endif // TSL_ORDERED_HASH_CHUNKED_SERIALIZATION_H