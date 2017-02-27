#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include "ordered_map.h"
#include "utils.h"

BOOST_AUTO_TEST_SUITE(test_ordered_map)


using test_types = boost::mpl::list<tsl::ordered_map<int64_t, int64_t>, 
                                    tsl::ordered_map<int64_t, int64_t, std::hash<int64_t>, std::equal_to<int64_t>, 
                                                     std::allocator<std::pair<int64_t, int64_t>>, 
                                                     std::deque<std::pair<int64_t, int64_t>>>, 
                                    tsl::ordered_map<int64_t, int64_t, std::hash<int64_t>, std::equal_to<int64_t>, 
                                                     std::allocator<std::pair<int64_t, int64_t>>, 
                                                     std::vector<std::pair<int64_t, int64_t>>>,
                                    tsl::ordered_map<int64_t, int64_t, mod_hash<9>>,
                                    tsl::ordered_map<std::string, self_reference_member_test, mod_hash<9>>,
                                    tsl::ordered_map<move_only_test, move_only_test, mod_hash<9>>,
                                    tsl::ordered_map<self_reference_member_test, self_reference_member_test, mod_hash<9>>
                                    >;

/**
 * insert
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HMap, test_types) {
    // insert x values, insert them again, check values through find, check order through iterator
    using key_t = typename HMap::key_type; using value_t = typename HMap:: mapped_type;
    
    const size_t nb_values = 1000;
    
    HMap map;
    typename HMap::iterator it;
    bool inserted;
    
    for(size_t i = 0; i < nb_values; i++) {
        const size_t insert_val = (i%2 == 0)?i:nb_values + i;
        std::tie(it, inserted) = map.insert({utils::get_key<key_t>(insert_val), utils::get_value<value_t>(insert_val)});
        
        BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(insert_val));
        BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(insert_val));
        BOOST_CHECK(inserted);
    }
    BOOST_CHECK_EQUAL(map.size(), nb_values);
    
    for(size_t i = 0; i < nb_values; i++) {
        const size_t insert_val = (i%2 == 0)?i:nb_values + i;
        std::tie(it, inserted) = map.insert({utils::get_key<key_t>(insert_val), utils::get_value<value_t>(insert_val)});
        
        BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(insert_val));
        BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(insert_val));
        BOOST_CHECK(!inserted);
    }
    BOOST_CHECK_EQUAL(map.size(), nb_values);
    
    for(size_t i = 0; i < nb_values; i++) {
        const size_t insert_val = (i%2 == 0)?i:nb_values + i;
        it = map.find(utils::get_key<key_t>(insert_val));
        
        BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(insert_val));
        BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(insert_val));
    }
    
    std::size_t i = 0;
    for(const auto& key_value: map) {
        const size_t insert_val = (i%2 == 0)?i:nb_values + i;
        
        BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_t>(insert_val));
        BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_t>(insert_val));
        
        i++;
    }
}



/**
 * erase
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_all, HMap, test_types) {
    // insert x values, delete all
    const size_t nb_values = 1000;
    HMap map = utils::get_filled_hash_map<HMap>(nb_values);
    
    auto it = map.erase(map.begin(), map.end());
    BOOST_CHECK(it == map.end());
    BOOST_CHECK(map.empty());
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_erase_loop, HMap, test_types) {
    // insert x values, delete all one by one
    size_t nb_values = 1000;
    HMap map = utils::get_filled_hash_map<HMap>(nb_values);
    HMap map2 = utils::get_filled_hash_map<HMap>(nb_values);
    
    auto it = map.begin();
    // Use second map to check for key after delete as we may not copy the key with move-only types.
    auto it2 = map2.begin();
    while(it != map.end()) {
        it = map.erase(it);
        --nb_values;
        
        BOOST_CHECK_EQUAL(map.count(it2->first), 0);
        BOOST_CHECK_EQUAL(map.size(), nb_values);
        ++it2;
    }
    
    BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert_erase_insert, HMap, test_types) {
    // insert x/2 values, delete x/4 values, insert x/2 values, find each value, check order of values
    using key_t = typename HMap::key_type; using value_t = typename HMap:: mapped_type;
    
    const size_t nb_values = 2000;
    HMap map;
    typename HMap::iterator it;
    bool inserted;
    
    // Insert
    for(size_t i = 0; i < nb_values/2; i++) {
        std::tie(it, inserted) = map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});
        
        BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
        BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
        BOOST_CHECK(inserted);
    }
    BOOST_CHECK_EQUAL(map.size(), nb_values/2);
    
    
    // Delete half
    for(size_t i = 0; i < nb_values/2; i++) {
        if(i%2 == 0) {
            BOOST_CHECK_EQUAL(map.erase(utils::get_key<key_t>(i)), 1);
        }
    }
    BOOST_CHECK_EQUAL(map.size(), nb_values/4);
    
    // Insert
    for(size_t i = nb_values/2; i < nb_values; i++) {
        std::tie(it, inserted) = map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});
        
        BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
        BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
        BOOST_CHECK(inserted);
    }
    BOOST_CHECK_EQUAL(map.size(), nb_values-nb_values/4);
    
    // Find
    for(size_t i = 0; i < nb_values; i++) {
        if(i%2 == 0 && i < nb_values/2) {
            it = map.find(utils::get_key<key_t>(i));
            
            BOOST_CHECK(it == map.cend());
        }
        else {
            it = map.find(utils::get_key<key_t>(i));
            
            BOOST_CHECK_EQUAL(it->first, utils::get_key<key_t>(i));
            BOOST_CHECK_EQUAL(it->second, utils::get_value<value_t>(i));
        }
    }
    
    // Order
    std::size_t i = 1;
    for(const auto& key_value: map) {
        if(i < nb_values/2) {
            BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_t>(i));
            BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_t>(i));
            
            i = std::min(i+2, nb_values/2);
        }
        else {
            BOOST_CHECK_EQUAL(key_value.first, utils::get_key<key_t>(i));
            BOOST_CHECK_EQUAL(key_value.second, utils::get_value<value_t>(i));
            
            i++;
        }
    }
}



/**
 * other
 */
BOOST_AUTO_TEST_CASE(test_heterogeneous_lookups) {
    struct hash_ptr {
        size_t operator()(const std::unique_ptr<int>& p) const {
            return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(p.get()));
        }
        
        size_t operator()(uintptr_t p) const {
            return std::hash<uintptr_t>()(p);
        }
        
        size_t operator()(const int* const& p) const {
            return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(p));
        }
    };
    
    struct equal_to_ptr {
        using is_transparent = std::true_type;
        
        bool operator()(const std::unique_ptr<int>& p1, const std::unique_ptr<int>& p2) const {
            return p1 == p2;
        }
        
        bool operator()(const std::unique_ptr<int>& p1, uintptr_t p2) const {
            return reinterpret_cast<uintptr_t>(p1.get()) == p2;
        }
        
        bool operator()(uintptr_t p1, const std::unique_ptr<int>& p2) const {
            return p1 == reinterpret_cast<uintptr_t>(p2.get());
        }
        
        bool operator()(const std::unique_ptr<int>& p1, const int* const& p2) const {
            return p1.get() == p2;
        }
        
        bool operator()(const int* const& p1, const std::unique_ptr<int>& p2) const {
            return p1 == p2.get();
        }
    };
    
    std::unique_ptr<int> ptr1(new int(1));
    std::unique_ptr<int> ptr2(new int(2));
    std::unique_ptr<int> ptr3(new int(3));
    int other;
    
    const uintptr_t addr1 = reinterpret_cast<uintptr_t>(ptr1.get());
    const int* const addr2 = ptr2.get();
    const int* const addr_unknown = &other;
     
    tsl::ordered_map<std::unique_ptr<int>, int, hash_ptr, equal_to_ptr> map;
    map.insert({std::move(ptr1), 4});
    map.insert({std::move(ptr2), 5});
    map.insert({std::move(ptr3), 6});
    
    BOOST_CHECK_EQUAL(map.size(), 3);
    
    
    BOOST_CHECK_EQUAL(map.at(addr1), 4);
    BOOST_CHECK_EQUAL(map.at(addr2), 5);
    BOOST_CHECK_THROW(map.at(addr_unknown), std::out_of_range);
    
    
    BOOST_CHECK_EQUAL(*map.find(addr1)->first, 1);
    BOOST_CHECK_EQUAL(*map.find(addr2)->first, 2);
    BOOST_CHECK(map.find(addr_unknown) == map.end());
    
    
    BOOST_CHECK_EQUAL(map.count(addr1), 1);
    BOOST_CHECK_EQUAL(map.count(addr2), 1);
    BOOST_CHECK_EQUAL(map.count(addr_unknown), 0);
    
    
    BOOST_CHECK_EQUAL(map.erase(addr1), 1);
    BOOST_CHECK_EQUAL(map.erase(addr2), 1);
    BOOST_CHECK_EQUAL(map.erase(addr_unknown), 0);
    
    
    BOOST_CHECK_EQUAL(map.size(), 1);
}


BOOST_AUTO_TEST_SUITE_END()
