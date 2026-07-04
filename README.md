# MiniVector.hpp

A fixed-capacity vector that never touches the heap -- capacity is a compile-time template
parameter, and the backing storage lives inline in the object itself (on the stack, or wherever
you put it). Built for code that constructs and destroys a lot of small vectors at high
frequency, where `std::vector`'s heap allocation is the actual bottleneck: in a microbenchmark of
building, filling, and destroying a 4-element vector in a tight loop, `MiniVector` measured
roughly 5x faster than a pre-`reserve()`d `std::vector`, and roughly 16x faster than a naively
grown one. See [Performance](#performance) below.

```cpp
#include "MiniVector.hpp"

MiniVector<int, 4> v;         // capacity 4, fixed at compile time
v.push_back(1);
v.push_back(2);
v.emplace_back(3);            // constructs T in place
v.insert(1, 99);              // {1, 99, 2, 3}
v.erase(0);                   // {99, 2, 3}
v.size();                     // 3
v.capacity();                 // 4
```

## Requirements

- C++20 or later
- Header-only -- copy `MiniVector.hpp` into your project and `#include` it
- `T` must be default-constructible and copy/move-assignable; see
  [Trivially copyable vs. not](#trivially-copyable-vs-not) below for how that affects `insert()`/`erase()`
- Optional: [`Ensure.hpp`](https://github.com/lrmoorejr/ensure) for its `ensure()` helper; if
  it's not present, `MiniVector` falls back to `assert()`

## API

| Call | Behavior |
|---|---|
| `MiniVector<T, M>()` | Default-constructs, empty, capacity `M`. |
| `MiniVector<T, M>(count)` | *explicit.* `count` value-initialized elements. |
| `MiniVector<T, M>(count, value)` | *explicit.* `count` copies of `value`. |
| `MiniVector<T, M>{a, b, c, ...}` | From a brace-init list. |
| `push_back(item)` | Appends a copy. |
| `emplace_back(args...)` | Constructs `T(args...)` in place (or default-constructs, given no args) and returns a reference to it. |
| `push()` | Reserves the next slot and returns a reference to it *without* constructing anything -- cheaper than `emplace_back()` for trivial `T`, but leaves the slot's prior contents as-is. |
| `pop_back()` | Removes the last element. |
| `insert(index, item)` | Inserts at `index`, shifting later elements over. |
| `erase(index)` | Removes at `index`, shifting later elements down. |
| `resize(count)` | Grows (value-initializing new elements) or shrinks to `count`. |
| `clear()` | Empties the vector. |
| `operator[](index)`, `at(index)` | Element access; `at()` bounds-checks via `ensure()`. |
| `front()`, `back()` | First/last element. |
| `begin()`, `end()`, `data()` | Raw pointer access/iteration. |
| `size()`, `empty()`, `capacity()` | Current count, whether it's 0, and `M`. |
| `operator==`, `operator!=` | Compares only the first `size()` elements of each side. |
| `Hash`, `Equal` | Functors for use as a key in `std::unordered_map`/`std::unordered_set` -- likewise only look at the first `size()` elements. |

## Trivially copyable vs. not

`insert()`/`erase()` shift elements over; for a trivially copyable `T` this is done with a single
`memmove`, which is why `MiniVector` can be so much faster than `std::vector` for that case. For
a non-trivially-copyable `T` (e.g. `std::string`), the same operations fall back at compile time
to a per-element move-assignment loop, so they still work correctly -- just without the bulk-copy
speedup.

One caveat either way: like `pop_back()`, `erase()` only lowers the element count -- it doesn't
destroy the now-unreachable last slot. For a non-trivial `T` holding a resource, that resource
isn't released until the slot is overwritten by a later `push_back()`/`insert()`/`resize()`, or
the whole `MiniVector` is destroyed.

## Fixed capacity

`M` is a compile-time constant, not a runtime-resizable capacity. Exceeding it (or an
out-of-range `index`) triggers `ensure()`'s abort -- there's no reallocate-and-grow path, by
design: growth is exactly the heap traffic this class exists to avoid. Choose `M` to comfortably
fit your actual usage; note that copying a `MiniVector` copies all `M` backing elements
regardless of how many are actually in use, so an `M` much larger than typical usage will cost
you on copies what it saves you on allocation.

## Performance

Measured with Catch2's benchmark harness, building/filling/destroying a small vector in a tight
loop (Release build, `-O3`; see `MiniVector-test.cpp`'s `"MiniVector vs std::vector"` benchmark
case):

| Variant | Mean time |
|---|---|
| `std::vector<int>`, no `reserve()` | ~70-73 ns |
| `std::vector<int>`, `reserve()`d up front | ~22-24 ns |
| `MiniVector<int, 4>` | ~4.4 ns |

`std::vector` pays a heap allocation (and, without `reserve()`, possibly more than one as it
grows) on every pass through the loop; `MiniVector` never allocates at all. This is the scenario
`MiniVector` is for -- it is not a general replacement for `std::vector` when elements are large
relative to how many you actually store, or when you need runtime-resizable capacity.

## License

Apache License 2.0 -- see [LICENSE](LICENSE).
