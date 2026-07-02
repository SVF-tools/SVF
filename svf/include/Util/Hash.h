#ifndef Hash_H
#define Hash_H

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

namespace SVF
{

/// provide extra hash function for std::pair handling
template <class T> struct Hash;

template <class S, class T> struct Hash<std::pair<S, T>>
{
    // Pairing function from: http://szudzik.com/ElegantPairing.pdf
    static size_t szudzik(size_t a, size_t b)
    {
        return a > b ? b * b + a : a * a + a + b;
    }

    size_t operator()(const std::pair<S, T>& t) const
    {
        Hash<decltype(t.first)> first;
        Hash<decltype(t.second)> second;
        return szudzik(first(t.first), second(t.second));
    }
};

template <class T> struct Hash
{
    size_t operator()(const T& t) const
    {
        std::hash<T> h;
        return h(t);
    }
};

template <typename Key, typename Hash = Hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using Set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

template <typename Key, typename Value, typename Hash = Hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, Value>>>
using Map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

template <typename Key, typename Compare = std::less<Key>,
        typename Allocator = std::allocator<Key>>
using OrderedSet = std::set<Key, Compare, Allocator>;

template <typename Key, typename Value, typename Compare = std::less<Key>,
        typename Allocator = std::allocator<std::pair<const Key, Value>>>
using OrderedMap = std::map<Key, Value, Compare, Allocator>;

}  // namespace SVF

#endif // Hash_H
