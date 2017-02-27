## C++ hash map/set which preserves the order of insertion

The ordered-map library provides a hash map and a hash set which preserve the order of insertion in a way similar to Python's [OrderedDict](https://docs.python.org/3/library/collections.html#collections.OrderedDict). When iterating over the map, the values will be returned in the same order as they were inserted.

The values are stored contiguously, no holes in-between values, in an underlying structure. By default a `std::deque` is used, but it is also possible to  use a `std::vector`. This structure is directly accessible through the `values_container()` method and if the structure is a `std::vector`, the `data()` method is also provided to easily interact with C API.

The library provides a behaviour similar to a `std::vector` with unique values but with an average search access of O(1).

### Differences compare to `std::unordered_map`
- The iterators are `RandomAccessIterator`.
- Iterator invalidation behaves in a way closer to `std::vector` and `std::deque` (see [API](https://tessil.github.io/ordered-map/doc/html/) for details).
- Slow `erase` operation, it has a complexity of O(n). A faster O(1) version, `unordered_erase`, exists but it breaks the insertion order (see [API](https://tessil.github.io/ordered-map/doc/html/) for details).
- For iterators, `operator*()` and `operator->()` return a reference and a pointer to `const std::pair<Key, T>` instead of `std::pair<const Key, T>` making the value `T` not modifiable. To modify the value you have to call the `value()` method of the iterator to get a mutable reference. Example:
```c++
tsl::ordered_map<int, int> map = {{1, 1}, {2, 1}, {3, 1}};
for(auto it = map.begin(); it != map.end(); ++it) {
    //it->second = 2; // Illegal
    it.value() = 2; // Ok
}
```
- No support for some bucket related methods (like bucket_size, bucket, ...).

These differences also apply between `std::unordered_set` and `tsl::ordered_set`.

### Installation
To use ordered-map, just include the header [src/ordered_map.h](src/ordered_map.h) to your project. It's a **header-only** library.

The code should work with any C++11 standard-compliant compiler and has been tested with GCC 6.1, Clang 3.6 and Visual Studio 2015.

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
#include "ordered_map.h"

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
    set.reserve(5);
    
    set = {'3', '4', '9', '2'};
    set.erase('2');
    set.insert('1');
    set.insert('\0');
    
    // Get raw buffer for C API: 3491
    std::cout << atoi(set.data()) << std::endl;
}
```

### License

The code is licensed under the MIT license, see the [LICENSE file](LICENSE) for details.
