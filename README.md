# BoundedVector.hpp

[API docs](https://lrmoorejr.github.io/bounded-vector/)

A fixed-capacity vector that never touches the heap -- capacity is a compile-time template
parameter, and the backing storage lives inline in the object itself (on the stack, or wherever
you put it). It's the same idea as `boost::container::static_vector` -- a vector-like container
with a hard, compile-time-fixed maximum size and inline storage instead of a heap-allocated
buffer -- but as a single, dependency-free header instead of part of Boost.

Built for code that constructs and destroys a lot of small vectors at high frequency, where
`std::vector`'s heap allocation is the actual bottleneck: in a microbenchmark of building,
filling, and destroying a 4-element vector in a tight loop, **`BoundedVector` measured roughly 17x
faster than a pre-`reserve()`d `std::vector`, and roughly 50x faster than a naively grown one**. See
[Performance](#performance) below.

```cpp
#include "BoundedVector.hpp"

BoundedVector<int, 4> v;         // capacity 4, fixed at compile time
v.push_back(1);
v.push_back(2);
v.emplace_back(3);               // constructs T in place
v.insert(v.begin() + 1, 99);     // {1, 99, 2, 3}
v.erase(v.begin());              // {99, 2, 3}
v.size();                        // 3
v.capacity();                    // 4
```

## Drop-in for std::vector?

Often, yes -- for code that only uses the common subset: `push_back`, `emplace_back`,
`pop_back`, iterator-based `insert`/`erase`, `size`/`empty`/`capacity`, `front`/`back`,
`operator[]`/`at`, `begin`/`end`/`data`, range-`for`, and `operator==`. Method names and behavior
deliberately match `std::vector` wherever there's no reason not to, precisely so it can slot in
for that common subset with a find-and-replace of the type name.

Where it can't: there's no `reserve()`/`shrink_to_fit()` (capacity is fixed at compile time, not
runtime-adjustable), no allocator support, and exceeding capacity throws instead of reallocating.
See [Fixed capacity](#fixed-capacity) below.

## Requirements

- C++20 or later
- Header-only -- copy `BoundedVector.hpp` into your project and `#include` it
- `T` must be default-constructible and copy/move-assignable; see
  [Trivially copyable vs. not](#trivially-copyable-vs-not) below for how that affects `insert_at()`/`erase_at()`
- Optional: [`Ensure.hpp`](https://github.com/lrmoorejr/ensure) for its `throw_if()` helper; if
  it's not present, `BoundedVector` falls back to an equivalent local implementation

## API

| Call | Behavior |
|---|---|
| `BoundedVector<T, M>()` | Default-constructs, empty, capacity `M`. |
| `BoundedVector<T, M>(count)` | *explicit.* `count` value-initialized elements. |
| `BoundedVector<T, M>(count, value)` | *explicit.* `count` copies of `value`. |
| `BoundedVector<T, M>{a, b, c, ...}` | From a brace-init list. |
| `iterator`, `const_iterator` | Plain `T*`/`const T*` -- there's no separate iterator type, since the backing storage is already a contiguous array. |
| `push_back(item)` | Appends a copy. |
| `emplace_back(args...)` | Constructs `T(args...)` in place (or default-constructs, given no args) and returns a reference to it. |
| `pop_back()` | Removes the last element. |
| `insert(pos, item)` | Inserts before iterator `pos`, matching `std::vector::insert()`; returns an iterator to the inserted element. |
| `erase(pos)` | Removes the element at iterator `pos`, matching `std::vector::erase()`; returns an iterator to the next element. |
| `insert_at(index, item)` | Index-based equivalent of `insert()`. Not an overload of `insert()`: a bare `0` is simultaneously a valid index and a valid null `const_iterator`, so `insert(0, item)` would be ambiguous between the two if they shared a name. |
| `erase_at(index)` | Index-based equivalent of `erase()`, kept separate for the same reason as `insert_at()`. |
| `resize(count)` | Grows (value-initializing new elements) or shrinks to `count`. |
| `clear()` | Empties the vector. |
| `operator[](index)`, `at(index)` | Element access; `at()` bounds-checks, throwing `std::out_of_range`. |
| `front()`, `back()` | First/last element. |
| `begin()`, `end()`, `data()` | Iterator/raw pointer access/iteration. |
| `size()`, `empty()`, `capacity()` | Current count, whether it's 0, and `M`. |
| `operator==`, `operator!=` | Compares only the first `size()` elements of each side. |
| `Hash`, `Equal` | Functors for use as a key in `std::unordered_map`/`std::unordered_set` -- likewise only look at the first `size()` elements. |

Every check that validates caller input (a bad index, exceeding capacity) throws rather than
aborting: `std::out_of_range` for bad indices/positions and an empty-vector `pop_back()`,
`std::length_error` for exceeding capacity -- matching `std::vector::at()`'s own throwing
contract, and `boost::container::static_vector`'s choice of `std::length_error`/`bad_alloc` for a
full container.

## Trivially copyable vs. not

`insert_at()`/`erase_at()` (and therefore the iterator-based `insert()`/`erase()`, which delegate
to them) shift elements over; for a trivially copyable `T` this is done with a single `memmove`
-- the same trick most `std::vector` implementations already use internally for trivial types, so
this is parity with `std::vector`, not an edge over it. For a non-trivially-copyable `T` (e.g.
`std::string`), the same operations fall back at compile time to a per-element move-assignment
loop instead, so they still work correctly -- just without the bulk-copy shortcut. (The actual
performance advantage over `std::vector`, below, comes entirely from avoiding heap allocation,
not from this.)

One caveat either way: like `pop_back()`, `erase()`/`erase_at()` only lower the element count --
they don't destroy the now-unreachable last slot. For a non-trivial `T` holding a resource, that
resource isn't released until the slot is overwritten by a later
`push_back()`/`insert()`/`resize()`, or the whole `BoundedVector` is destroyed.

## Fixed capacity

`M` is a compile-time constant, not a runtime-resizable capacity. Exceeding it throws
`std::length_error`, and an out-of-range index/position throws `std::out_of_range` -- there's no
reallocate-and-grow path, by design: growth is exactly the heap traffic this class exists to
avoid. Choose `M` to comfortably fit your actual usage; note that copying a `BoundedVector`
copies all `M` backing elements regardless of how many are actually in use, so an `M` much larger
than typical usage will cost you on copies what it saves you on allocation.

## Performance

Measured with Catch2's benchmark harness, building/filling/destroying a small vector in a tight
loop (Release build, `-O3`; see `BoundedVector-test.cpp`'s `"BoundedVector vs std::vector"`
benchmark case):

| Variant | Mean time |
|---|---|
| `std::vector<int>`, no `reserve()` | ~67-70 ns |
| `std::vector<int>`, `reserve()`d up front | ~22-24 ns |
| `BoundedVector<int, 4>` | ~1.4 ns |

`std::vector` pays a heap allocation (and, without `reserve()`, possibly more than one as it
grows) on every pass through the loop; `BoundedVector` never allocates at all. This is the
scenario `BoundedVector` is for -- it is not a general replacement for `std::vector` when
elements are large relative to how many you actually store, or when you need runtime-resizable
capacity.

## License

Apache License 2.0 -- see [LICENSE](https://github.com/lrmoorejr/bounded-vector/blob/main/LICENSE).
