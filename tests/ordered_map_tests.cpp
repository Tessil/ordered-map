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

BOOST_AUTO_TEST_CASE(test_range_insert) {
    // insert x values in vector, range insert x-15 values from vector to map, check values
    const int nb_values = 1000;
    std::vector<std::pair<int, int>> values;
    for(int i = 0; i < nb_values; i++) {
        values.push_back(std::make_pair(i, i+1));
    }
    
    
    tsl::ordered_map<int, int> map = {{-1, 0}, {-2, 0}};
    map.insert(std::next(values.begin(), 10), std::prev(values.end(), 5));
    
    BOOST_CHECK_EQUAL(map.size(), 987);
    
    BOOST_CHECK_EQUAL(map.values_container()[0].first, -1);
    BOOST_CHECK_EQUAL(map.values_container()[0].second, 0);
    
    BOOST_CHECK_EQUAL(map.values_container()[1].first, -2);
    BOOST_CHECK_EQUAL(map.values_container()[1].second, 0);
    
    for(int i = 10, j = 2; i < nb_values - 5; i++, j++) {
        BOOST_CHECK_EQUAL(map.values_container()[j].first, i);
        BOOST_CHECK_EQUAL(map.values_container()[j].second, i+1);
    }
    
}


BOOST_AUTO_TEST_CASE(test_insert_with_hint) {
    tsl::ordered_map<int, int> map{{1, 0}, {2, 1}, {3, 2}};
    BOOST_CHECK(map.insert(map.find(2), std::make_pair(3, 4)) == map.find(3));
    BOOST_CHECK(map.insert(map.find(2), std::make_pair(2, 4)) == map.find(2));
    BOOST_CHECK(map.insert(map.find(10), std::make_pair(2, 4)) == map.find(2));
    
    BOOST_CHECK_EQUAL(map.size(), 3);
    BOOST_CHECK_EQUAL(map.insert(map.find(10), std::make_pair(4, 3))->first, 4);
    BOOST_CHECK_EQUAL(map.insert(map.find(2), std::make_pair(5, 4))->first, 5);
}

/**
 * emplace
 */
BOOST_AUTO_TEST_CASE(test_emplace) {
    tsl::ordered_map<int64_t, move_only_test> map;
    tsl::ordered_map<int64_t, move_only_test>::iterator it;
    bool inserted;
    
    
    std::tie(it, inserted) = map.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(10),
                                            std::forward_as_tuple(1));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    BOOST_CHECK(inserted);
    
    
    std::tie(it, inserted) = map.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(10),
                                            std::forward_as_tuple(3));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    BOOST_CHECK(!inserted);
}

/**
 * try_emplace
 */
BOOST_AUTO_TEST_CASE(test_try_emplace) {
    tsl::ordered_map<int64_t, move_only_test> map;
    tsl::ordered_map<int64_t, move_only_test>::iterator it;
    bool inserted;
    
    
    std::tie(it, inserted) = map.try_emplace(10, 1);
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    BOOST_CHECK(inserted);
    
    
    std::tie(it, inserted) = map.try_emplace(10, 3);
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    BOOST_CHECK(!inserted);
}

BOOST_AUTO_TEST_CASE(test_try_emplace_hint) {
    tsl::ordered_map<int64_t, move_only_test> map(0);
    
    auto it = map.try_emplace(map.find(10), 10, 1);
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    
    
    it = map.try_emplace(map.find(10), 10, 3);
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
}


/**
 * insert_or_assign
 */
BOOST_AUTO_TEST_CASE(test_insert_or_assign) {
    tsl::ordered_map<int64_t, move_only_test> map;
    tsl::ordered_map<int64_t, move_only_test>::iterator it;    
    bool inserted;
    
    
    std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(1));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    BOOST_CHECK(inserted);
    
    
    std::tie(it, inserted) = map.insert_or_assign(10, move_only_test(3));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(3));
    BOOST_CHECK(!inserted);
}


BOOST_AUTO_TEST_CASE(test_insert_or_assign_hint) {
    tsl::ordered_map<int64_t, move_only_test> map(0);
    
    auto it = map.insert_or_assign(map.find(10), 10, move_only_test(1));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(1));
    
    
    it = map.insert_or_assign(map.find(10), 10, move_only_test(3));
    BOOST_CHECK_EQUAL(it->first, 10);
    BOOST_CHECK_EQUAL(it->second, move_only_test(3));
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

BOOST_AUTO_TEST_CASE(test_range_erase_same_iterators) {
    const size_t nb_values = 100;
    auto map = utils::get_filled_hash_map<tsl::ordered_map<int64_t, int64_t>>(nb_values);
    
    tsl::ordered_map<int64_t, int64_t>::const_iterator it_const = map.cbegin();
    std::advance(it_const, 10);
    
    tsl::ordered_map<int64_t, int64_t>::iterator it_mutable = map.erase(it_const, it_const);
    BOOST_CHECK(it_const == it_mutable);
    BOOST_CHECK_EQUAL(map.size(), 100);
    
    it_mutable.value() = -100;
    BOOST_CHECK_EQUAL(it_const.value(), -100);
}

/**
 * unordered_erase
 */
BOOST_AUTO_TEST_CASE(test_unordered_erase) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}, 
                                              {4, 40}, {5, 50}, {6, 60}};
    BOOST_CHECK_EQUAL(map.size(), 6);
    
    
    BOOST_CHECK_EQUAL(map.unordered_erase(3), 1);
    BOOST_CHECK_EQUAL(map.size(), 5);
    
    BOOST_CHECK_EQUAL(map.unordered_erase(0), 0);
    BOOST_CHECK_EQUAL(map.size(), 5);
    
    
    auto it = map.begin();
    while(it != map.end()) {
        it = map.unordered_erase(it);
    }
    
    BOOST_CHECK_EQUAL(map.size(), 0);
}


/**
 * operator== and operator!=
 */
BOOST_AUTO_TEST_CASE(test_compare) {
    const tsl::ordered_map<std::string, int> map = {{"D", 1}, {"L", 2}, {"A", 3}};
    
    BOOST_ASSERT(map == (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"A", 3}}));
    BOOST_ASSERT(map != (tsl::ordered_map<std::string, int>{{"L", 2}, {"D", 1}, {"A", 3}}));
    
    
    BOOST_ASSERT(map < (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"B", 3}}));
    BOOST_ASSERT(map <= (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"B", 3}}));
    BOOST_ASSERT(map <= (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"A", 3}}));
    
    BOOST_ASSERT(map > (tsl::ordered_map<std::string, int>{{"D", 1}, {"K", 2}, {"A", 3}}));
    BOOST_ASSERT(map >= (tsl::ordered_map<std::string, int>{{"D", 1}, {"K", 2}, {"A", 3}}));
    BOOST_ASSERT(map >= (tsl::ordered_map<std::string, int>{{"D", 1}, {"L", 2}, {"A", 3}}));
}



/**
 * clear
 */
BOOST_AUTO_TEST_CASE(test_clear) {
    // insert x values, clear map
    const size_t nb_values = 1000;
    auto map = utils::get_filled_hash_map<tsl::ordered_map<int64_t, int64_t>>(nb_values);
    
    map.clear();
    BOOST_CHECK_EQUAL(map.size(), 0);
    BOOST_CHECK_EQUAL(std::distance(map.begin(), map.end()), 0);
    
    map.insert({5, -5});
    map.insert({{1, -1}, {2, -1}, {4, -4}, {3, -3}});
    
    BOOST_CHECK(map == (tsl::ordered_map<int64_t, int64_t>({{5, -5}, {1, -1}, {2, -1}, {4, -4}, {3, -3}})));
}


/**
 * iterator
 */
BOOST_AUTO_TEST_CASE(test_reverse_iterator) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 1}, {-2, 2}, {3, 3}};
    map[2] = 4;
    
    std::size_t i = 4;
    for(auto it = map.rbegin(); it != map.rend(); ++it) {
        BOOST_CHECK_EQUAL(it->second, i);
        i--;
    }
    
    i = 4;
    for(auto it = map.rcbegin(); it != map.rcend(); ++it) {
        BOOST_CHECK_EQUAL(it->second, i);
        i--;
    }
}

BOOST_AUTO_TEST_CASE(test_iterator_arithmetic) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}, 
                                              {4, 40}, {5, 50}, {6, 60}};
                                              
    tsl::ordered_map<int64_t, int64_t>::const_iterator it;
    tsl::ordered_map<int64_t, int64_t>::const_iterator it2;
    
    it = map.cbegin();
    // it += n
    it += 3;
    BOOST_CHECK_EQUAL(it->second, 40);
    
    
    
    // it + n
    BOOST_CHECK_EQUAL((map.cbegin() + 3)->second, 40);
    // n + it
    BOOST_CHECK_EQUAL((3 + map.cbegin())->second, 40);
    
    
    
    it = map.cbegin() + 4;
    // it -= n
    it -= 2;
    BOOST_CHECK_EQUAL(it->second, 30);
    
    
    
    // it - n
    BOOST_CHECK_EQUAL((it - 1)->second, 20);
    
    
    
    it = map.cbegin() + 2;
    it2 = map.cbegin() + 4;
    // it - it
    BOOST_CHECK_EQUAL(it2 - it, 2);
    
    
    
    // it[n]
    BOOST_CHECK_EQUAL(map.cbegin()[2].second, 30);
    
    it = map.cbegin() + 1;
    // it[n]
    BOOST_CHECK_EQUAL(it[2].second, 40);
    
    
    
    it = map.cbegin();
    // it++
    it++;
    BOOST_CHECK_EQUAL((it++)->second, 20);
    
    
    it = map.cbegin();
    // ++it
    ++it;
    BOOST_CHECK_EQUAL((++it)->second, 30);
    
    
    
    it = map.cend();
    // it--
    it--;
    BOOST_CHECK_EQUAL((it--)->second, 60);
    
    
    it = map.cend();
    // --it
    --it;
    BOOST_CHECK_EQUAL((--it)->second, 50);
}

BOOST_AUTO_TEST_CASE(test_iterator_compartors) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}, 
                                              {4, 40}, {5, 50}, {6, 60}};
                                              
    tsl::ordered_map<int64_t, int64_t>::const_iterator it;
    tsl::ordered_map<int64_t, int64_t>::const_iterator it2;
    
    it = map.begin() + 1;
    it2 = map.end() - 1;
    
    BOOST_CHECK(it < it2);
    BOOST_CHECK(it <= it2);
    BOOST_CHECK(it2 > it);
    BOOST_CHECK(it2 >= it);
    
    it = map.begin() + 3;
    it2 = map.end() - 3;
    
    BOOST_CHECK(it == it2);
    BOOST_CHECK(it <= it2);
    BOOST_CHECK(it >= it2);
    BOOST_CHECK(it2 <= it);
    BOOST_CHECK(it2 >= it);
    BOOST_CHECK(!(it < it2));
    BOOST_CHECK(!(it > it2));
    BOOST_CHECK(!(it2 < it));
    BOOST_CHECK(!(it2 > it));
}


/**
 * iterator.value()
 */
BOOST_AUTO_TEST_CASE(test_modify_value) {
    // insert x values, modify value of even keys, check values
    const size_t nb_values = 100;
    auto map = utils::get_filled_hash_map<tsl::ordered_map<int64_t, int64_t>>(nb_values);
    
    for(auto it = map.begin(); it != map.end(); ++it) {
        if(it->first % 2 == 0) {
            it.value() = -1;
        }
    }
    
    for(auto& val : map) {
        if(val.first % 2 == 0) {
            BOOST_CHECK_EQUAL(val.second, -1);
        }
        else {
            BOOST_CHECK_NE(val.second, -1);
        }
    }
}


/**
 * operator=
 */
BOOST_AUTO_TEST_CASE(test_assign_operator) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}};
    BOOST_CHECK_EQUAL(map.size(), 3);
    
    map = {{4, 40}, {5, 50}};
    BOOST_CHECK_EQUAL(map.size(), 2);
    BOOST_CHECK_EQUAL(map.at(4), 40);
}


BOOST_AUTO_TEST_CASE(test_copy) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}};
    tsl::ordered_map<int64_t, int64_t> map2 = map;
    tsl::ordered_map<int64_t, int64_t> map3;
    map3 = map;
    
    BOOST_CHECK(map == map2);
    BOOST_CHECK(map == map3);
    BOOST_CHECK_EQUAL(map.size(), 3);
}


BOOST_AUTO_TEST_CASE(test_move) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {2, 20}, {3, 30}};
    tsl::ordered_map<int64_t, int64_t> map2 = std::move(map);
    
    BOOST_CHECK(map.empty());
    BOOST_CHECK(map.begin() == map.end());
    BOOST_CHECK_EQUAL(map2.size(), 3);
    BOOST_CHECK(map2 == (tsl::ordered_map<int64_t, int64_t>{{1, 10}, {2, 20}, {3, 30}}));
    
    
    tsl::ordered_map<int64_t, int64_t> map3;
    map3 = std::move(map2);
    
    BOOST_CHECK(map2.empty());
    BOOST_CHECK(map2.begin() == map2.end());
    BOOST_CHECK_EQUAL(map3.size(), 3);
    BOOST_CHECK(map3 == (tsl::ordered_map<int64_t, int64_t>{{1, 10}, {2, 20}, {3, 30}}));
    
    map2 = {{1, 10}, {2, 20}};
    BOOST_CHECK(map2 == (tsl::ordered_map<int64_t, int64_t>{{1, 10}, {2, 20}}));
}


/**
 * at
 */
BOOST_AUTO_TEST_CASE(test_at) {
    // insert x values, use at for known and unknown values.
    tsl::ordered_map<int64_t, int64_t> map = {{0, 10}, {-2, 20}};
    
    BOOST_CHECK_EQUAL(map.at(0), 10);
    BOOST_CHECK_EQUAL(map.at(-2), 20);
    BOOST_CHECK_THROW(map.at(1), std::out_of_range);
}


/**
 * data()
 */
BOOST_AUTO_TEST_CASE(test_data) {
    tsl::ordered_map<int64_t, int64_t, std::hash<int64_t>, std::equal_to<int64_t>, 
                    std::allocator<std::pair<int64_t, int64_t>>, 
                    std::vector<std::pair<int64_t, int64_t>>> map = {{1, -1}, {2, -2}, {4, -4}, {3, -3}};
    
    BOOST_CHECK(map.data() == map.values_container().data());
}

/**
 * operator[]
 */
BOOST_AUTO_TEST_CASE(test_access_operator) {
    // insert x values, use at for known and unknown values.
    tsl::ordered_map<int64_t, int64_t> map = {{0, 10}, {-2, 20}};
    
    BOOST_CHECK_EQUAL(map[0], 10);
    BOOST_CHECK_EQUAL(map[-2], 20);
    BOOST_CHECK_EQUAL(map[2], int64_t());
    
    BOOST_CHECK_EQUAL(map.size(), 3);
}

/**
 * swap
 */
BOOST_AUTO_TEST_CASE(test_swap) {
    tsl::ordered_map<int64_t, int64_t> map = {{1, 10}, {8, 80}, {3, 30}};
    tsl::ordered_map<int64_t, int64_t> map2 = {{4, 40}, {5, 50}};
    
    using std::swap;
    swap(map, map2);
    
    BOOST_CHECK(map == (tsl::ordered_map<int64_t, int64_t>{{4, 40}, {5, 50}}));
    BOOST_CHECK(map2 == (tsl::ordered_map<int64_t, int64_t>{{1, 10}, {8, 80}, {3, 30}}));
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
    BOOST_CHECK_EQUAL(map.unordered_erase(addr2), 1);
    BOOST_CHECK_EQUAL(map.erase(addr_unknown), 0);
    BOOST_CHECK_EQUAL(map.unordered_erase(addr_unknown), 0);
    
    
    BOOST_CHECK_EQUAL(map.size(), 1);
}



/**
 * Various operations on empty map
 */
BOOST_AUTO_TEST_CASE(test_empty_map) {
    tsl::ordered_map<std::string, int> map(0);
    
    BOOST_CHECK_EQUAL(map.size(), 0);
    BOOST_CHECK(map.empty());
    
    BOOST_CHECK(map.begin() == map.end());
    BOOST_CHECK(map.begin() == map.cend());
    BOOST_CHECK(map.cbegin() == map.cend());
    
    BOOST_CHECK(map.find("") == map.end());
    BOOST_CHECK(map.find("test") == map.end());
    
    BOOST_CHECK_EQUAL(map.count(""), 0);
    BOOST_CHECK_EQUAL(map.count("test"), 0);
    
    BOOST_CHECK_THROW(map.at(""), std::out_of_range);
    BOOST_CHECK_THROW(map.at("test"), std::out_of_range);
    
    auto range = map.equal_range("test");
    BOOST_CHECK(range.first == range.second);
    
    BOOST_CHECK_EQUAL(map.erase("test"), 0);
    BOOST_CHECK(map.erase(map.begin(), map.end()) == map.end());
    
    BOOST_CHECK_EQUAL(map["new value"], int{});
}


BOOST_AUTO_TEST_SUITE_END()
