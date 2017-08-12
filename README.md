[![Build Status](https://travis-ci.org/Tessil/ordered-map.svg?branch=master)](https://travis-ci.org/Tessil/ordered-map) [![Build status](https://ci.appveyor.com/api/projects/status/7fug7piv59d0in36/branch/master?svg=true)](https://ci.appveyor.com/project/Tessil/ordered-map/branch/master)
## C++ hash map and hash set which preserves the order of insertion

The ordered-map library provides a hash map and a hash set which preserve the order of insertion in a way similar to Python's [OrderedDict](https://docs.python.org/3/library/collections.html#collections.OrderedDict). When iterating over the map, the values will be returned in the same order as they were inserted.

The values are stored contiguously in an underlying structure, no holes in-between values even after an erase operation. By default a `std::deque` is used for this structure, but it's also possible to  use a `std::vector`. This structure is directly accessible through the `values_container()` method and if the structure is a `std::vector`, a `data()` method is also provided to easily interact with C APIs.

To resolve collisions on hashes, the library uses robin hood probing with backward shift deletion.

The library provides a behaviour similar to a `std::deque/std::vector` with unique values but with an average time complexity of O(1) for lookups and an amortised time complexity of O(1) for insertions. This comes at the price of a little higher memory footprint (8 bytes per entry if the load factor is 1, around 16 bytes per entry for a 0.5 load factor).

Two classes are provided: `tsl::ordered_map` and `tsl::ordered_set`.

**Note**: The library uses a power of two for the size of its buckets array to take advantage of the [fast modulo](https://en.wikipedia.org/wiki/Modulo_operation#Performance_issues). For good performance, it requires the hash table to have a well-distributed hash function. If you encounter performance issues check your hash function.

### Key features
- Header-only library, just add the project to your include path and you are ready to go.
- Values are stored in the same order as the insertion order. The library provides a direct access to the underlying structure which stores the values.
- O(1) average time complexity for lookups with performances similar to `std::unordered_map` but with faster insertions and reduced memory usage.
- Provide random access iterators and also reverse iterators.
- Support for heterogeneous lookups (e.g. if you have a map that uses `std::unique_ptr<int>` as key, you could use an `int*` or a `std::uintptr_t` for example as key parameter for `find`, see [example](https://github.com/Tessil/ordered-map#heterogeneous-lookup)).
- API closely similar to `std::unordered_map` and `std::unordered_set`.

### Differences compare to `std::unordered_map`
- The iterators are `RandomAccessIterator`.
- Iterator invalidation behaves in a way closer to `std::vector` and `std::deque` (see [API](https://tessil.github.io/ordered-map/doc/html/) for details). If you use `std::vector` as `ValueTypeContainer`, you can use `reserve()` to preallocate some space and avoid the invalidation of the iterators on insert.
- Slow `erase()` operation, it has a complexity of O(n). A faster O(1) version `unordered_erase()` exists, but it breaks the insertion order (see [API](https://tessil.github.io/ordered-map/doc/html/) for details). An O(1) `pop_back()` is also available.
- For iterators, `operator*()` and `operator->()` return a reference and a pointer to `const std::pair<Key, T>` instead of `std::pair<const Key, T>` making the value `T` not modifiable. To modify the value you have to call the `value()` method of the iterator to get a mutable reference. Example:
```c++
tsl::ordered_map<int, int> map = {{1, 1}, {2, 1}, {3, 1}};
for(auto it = map.begin(); it != map.end(); ++it) {
    //it->second = 2; // Illegal
    it.value() = 2; // Ok
}
```
- The map can only hold up to 2<sup>32</sup> - 1 values, that is 4 294 967 295 values.
- No support for some bucket related methods (like bucket_size, bucket, ...).


Thread-safety guarantee is the same as `std::unordered_map` (possible to have multiple readers). Concerning the strong exception guarantee, it holds only if `ValueContainer::emplace_back` has the strong exception guarantee (which is true for `std::vector` and `std::deque` as long as the type T is not a move-only type with a move constructor that may throw an exception, see [details](http://en.cppreference.com/w/cpp/container/vector/emplace_back#Exceptions)).

These differences also apply between `std::unordered_set` and `tsl::ordered_set`.


### Installation
To use ordered-map, just add the project to your include path. It is a **header-only** library.

The code should work with any C++11 standard-compliant compiler and has been tested with GCC 4.8.4, Clang 3.5.0 and Visual Studio 2015.

To run the tests you will need the Boost Test library and CMake.

```bash
git clone https://github.com/Tessil/ordered-map.git
cd ordered-map
mkdir build
cd build
cmake ..
make
./test_ordered_map
```

### Usage
The API can be found [here](https://tessil.github.io/ordered-map/doc/html/).

### Example
```c++
#include <iostream>
#include <string>
#include <cstdlib>
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>

int main() {
    tsl::ordered_map<char, int> map = {{'d', 1}, {'a', 2}, {'g', 3}};
    map.insert({'b', 4});
    map['h'] = 5;
    map['e'] = 6;
    
    map.erase('a');
    
    
    // {d, 1} {g, 3} {b, 4} {h, 5} {e, 6}
    for(const auto& key_value : map) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
    
    map.unordered_erase('b');
    
    // Break order: {d, 1} {g, 3} {e, 6} {h, 5}
    for(const auto& key_value : map) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
    
    tsl::ordered_set<char, std::hash<char>, std::equal_to<char>,
                     std::allocator<char>, std::vector<char>> set;
    set.reserve(6);
    
    set = {'3', '4', '9', '2'};
    set.erase('2');
    set.insert('1');
    set.insert('\0');
    
    set.pop_back();
    set.insert({'0', '\0'});
    
    // Get raw buffer for C API: 34910
    std::cout << atoi(set.data()) << std::endl;
}
```

#### Heterogeneous lookup

Heterogeneous overloads allow the usage of other types than `Key` for lookup and erase operations as long as the used types are hashable and comparable to `Key`.

To activate the heterogeneous overloads in `tsl::ordered_map/set`, the qualified-id `KeyEqual::is_transparent` must be valid. It works the same way as for [`std::map::find`](http://en.cppreference.com/w/cpp/container/map/find). You can either use [`std::equal_to<>`](http://en.cppreference.com/w/cpp/utility/functional/equal_to_void) or define your own function object.

Both `KeyEqual` and `Hash` will need to be able to deal with the different types.

```c++
#include <functional>
#include <iostream>
#include <string>
#include <tsl/ordered_map.h>


struct employee {
    employee(int id, std::string name) : m_id(id), m_name(std::move(name)) {
    }
    
    friend bool operator==(const employee& empl, int empl_id) {
        return empl.m_id == empl_id;
    }
    
    friend bool operator==(int empl_id, const employee& empl) {
        return empl_id == empl.m_id;
    }
    
    friend bool operator==(const employee& empl1, const employee& empl2) {
        return empl1.m_id == empl2.m_id;
    }
    
    
    int m_id;
    std::string m_name;
};

struct hash_employee {
    std::size_t operator()(const employee& empl) const {
        return std::hash<int>()(empl.m_id);
    }
    
    std::size_t operator()(int id) const {
        return std::hash<int>()(id);
    }
};

struct equal_employee {
    using is_transparent = void;
    
    bool operator()(const employee& empl, int empl_id) const {
        return empl.m_id == empl_id;
    }
    
    bool operator()(int empl_id, const employee& empl) const {
        return empl_id == empl.m_id;
    }
    
    bool operator()(const employee& empl1, const employee& empl2) const {
        return empl1.m_id == empl2.m_id;
    }
};


int main() {
    // Use std::equal_to<> which will automatically deduce and forward the parameters
    tsl::ordered_map<employee, int, hash_employee, std::equal_to<>> map; 
    map.insert({employee(1, "John Doe"), 2001});
    map.insert({employee(2, "Jane Doe"), 2002});
    map.insert({employee(3, "John Smith"), 2003});

    // John Smith 2003
    auto it = map.find(3);
    if(it != map.end()) {
        std::cout << it->first.m_name << " " << it->second << std::endl;
    }

    map.erase(1);



    // Use a custom KeyEqual which has an is_transparent member type
    tsl::ordered_map<employee, int, hash_employee, equal_employee> map2;
    map2.insert({employee(4, "Johnny Doe"), 2004});

    // 2004
    std::cout << map2.at(4) << std::endl;
} 
```

### License

The code is licensed under the MIT license, see the [LICENSE file](LICENSE) for details.
