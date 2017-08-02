#ifndef TSL_UTILS_H
#define TSL_UTILS_H

#include <boost/numeric/conversion/cast.hpp>

template<unsigned int MOD>
class mod_hash {
public:   
    template<typename T>
    size_t operator()(const T&  value) const {
        return std::hash<T>()(value) % MOD;
    }
};


class move_only_test {
public:
    explicit move_only_test(int64_t value) : m_value(new int64_t(value)) {
    }
    
    move_only_test(const move_only_test&) = delete;
    move_only_test(move_only_test&&) = default;
    move_only_test& operator=(const move_only_test&) = delete;
    move_only_test& operator=(move_only_test&&) = default;
    
    friend std::ostream& operator<<(std::ostream& stream, const move_only_test& value) {
        if(value.m_value == nullptr) {
            stream << "null";
        }
        else {
            stream << *value.m_value;
        }
        
        return stream;
    }
    
    friend bool operator==(const move_only_test& lhs, const move_only_test& rhs) { 
        if(lhs.m_value == nullptr || rhs.m_value == nullptr) {
            return lhs.m_value == nullptr && lhs.m_value == nullptr;
        }
        else {
            return *lhs.m_value == *rhs.m_value; 
        }
    }
    
    friend bool operator!=(const move_only_test& lhs, const move_only_test& rhs) { 
        return !(lhs == rhs); 
    }
    
    int64_t value() const {
        return *m_value;
    }
private:    
    std::unique_ptr<int64_t> m_value;
};


namespace std {
    template<>
    struct hash<move_only_test> {
        size_t operator()(const move_only_test& val) const {
            return std::hash<int64_t>()(val.value());
        }
    };
};


class utils {
public:
    template<typename T>
    static T get_key(size_t counter);
    
    template<typename T>
    static T get_value(size_t counter);
    
    template<typename HMap>
    static HMap get_filled_hash_map(size_t nb_elements);
};



template<>
inline int64_t utils::get_key<int64_t>(size_t counter) {
    return boost::numeric_cast<int64_t>(counter);
}

template<>
inline std::string utils::get_key<std::string>(size_t counter) {
    return "Key " + std::to_string(counter);
}

template<>
inline move_only_test utils::get_key<move_only_test>(size_t counter) {
    return move_only_test(boost::numeric_cast<int64_t>(counter));
}




template<>
inline int64_t utils::get_value<int64_t>(size_t counter) {
    return boost::numeric_cast<int64_t>(counter*2);
}

template<>
inline std::string utils::get_value<std::string>(size_t counter) {
    return "Value " + std::to_string(counter);
}

template<>
inline move_only_test utils::get_value<move_only_test>(size_t counter) {
    return move_only_test(boost::numeric_cast<int64_t>(counter*2));
}



template<typename HMap>
inline HMap utils::get_filled_hash_map(size_t nb_elements) {
    using key_t = typename HMap::key_type; using value_t = typename HMap:: mapped_type;
    
    HMap map;
    map.reserve(nb_elements);
    
    for(size_t i = 0; i < nb_elements; i++) {
        map.insert({utils::get_key<key_t>(i), utils::get_value<value_t>(i)});
    }
    
    return map;
}

#endif
