#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "utils.h"
#include "ordered_set.h"


BOOST_AUTO_TEST_SUITE(test_ordered_set)


using test_types = boost::mpl::list<tsl::ordered_set<std::int64_t, std::hash<std::int64_t>, std::equal_to<std::int64_t>, 
                                                     std::allocator<std::int64_t>, 
                                                     std::deque<std::int64_t>>, 
                                    tsl::ordered_set<std::int64_t, std::hash<std::int64_t>, std::equal_to<std::int64_t>, 
                                                     std::allocator<std::int64_t>, 
                                                     std::vector<std::int64_t>>, 
                                    tsl::ordered_set<std::int64_t, mod_hash<9>>, 
                                    tsl::ordered_set<std::string>,
                                    tsl::ordered_set<std::string, mod_hash<9>>, 
                                    tsl::ordered_set<move_only_test, mod_hash<9>>
                                    >;

/**
 * insert
 */
BOOST_AUTO_TEST_CASE_TEMPLATE(test_insert, HSet, test_types) {
    // insert x values, insert them again, check values through find, check order through iterator
    using key_t = typename HSet::key_type;
    
    const std::size_t nb_values = 1000;
    
    HSet set;
    typename HSet::iterator it;
    bool inserted;
    
    for(std::size_t i = 0; i < nb_values; i++) {
        const std::size_t insert_val = (i%2 == 0)?i:nb_values + i;
        std::tie(it, inserted) = set.insert(utils::get_key<key_t>(insert_val));
        
        BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
        BOOST_CHECK(inserted);
    }
    BOOST_CHECK_EQUAL(set.size(), nb_values);
    
    for(std::size_t i = 0; i < nb_values; i++) {
        const std::size_t insert_val = (i%2 == 0)?i:nb_values + i;
        std::tie(it, inserted) = set.insert(utils::get_key<key_t>(insert_val));
        
        BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
        BOOST_CHECK(!inserted);
    }
    BOOST_CHECK_EQUAL(set.size(), nb_values);
    
    for(std::size_t i = 0; i < nb_values; i++) {
        const std::size_t insert_val = (i%2 == 0)?i:nb_values + i;
        it = set.find(utils::get_key<key_t>(insert_val));
        
        BOOST_CHECK_EQUAL(*it, utils::get_key<key_t>(insert_val));
    }
    
    std::size_t i = 0;
    for(const auto& value: set) {
        const std::size_t insert_val = (i%2 == 0)?i:nb_values + i;
        
        BOOST_CHECK_EQUAL(value, utils::get_key<key_t>(insert_val));
        
        i++;
    }
}



BOOST_AUTO_TEST_SUITE_END()
