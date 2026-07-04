#pragma once

/*
 * Copyright 2026 L. Richard Moore Jr.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*

A lightweight Vector object that does not draw from the heap

*/

#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Ensure.hpp is an optional dependency: if it's available (either as part of this
// checkout or vendored alongside this header), use its throw_if() for input
// validation; otherwise fall back to an equivalent local implementation so this
// header still works standalone. Unlike ensure() (this codebase's usual choice for
// "should never happen" internal invariants), every check in this header validates
// caller-supplied input (an index, a requested size) that a caller may legitimately
// want to catch and recover from -- matching std::vector::at()'s own throwing
// contract -- so throw_if() rather than ensure() is used throughout.
#if __has_include("commons/Ensure.hpp")
	#include "commons/Ensure.hpp"
#elif __has_include("Ensure.hpp")
	#include "Ensure.hpp"
#else
	// Guard against Ensure.hpp having already been included under a path our
	// __has_include checks above don't know about (e.g. vendored elsewhere as
	// "3rdparty/Ensure.hpp") -- COMMONS_ENSURE_HPP is defined by Ensure.hpp
	// itself, so this still catches that case even under an unknown filename.
	// (throw_if is a function template, not a macro, so #ifndef can't guard
	// it directly -- redefining it without this check would be a hard error.)
	#ifndef COMMONS_ENSURE_HPP
	template<class T, class... Args>
	constexpr inline void throw_if(bool condition, Args&&... args) {
		if (condition)
			throw T(std::forward<Args>(args)...);
	}
	#endif
#endif

template<typename T, short M>
class BoundedVector {
	// components (the full M-slot array) is default-initialized on entry to every constructor
	// that doesn't explicitly list it in its member-initializer list -- which is all of them
	// except the default constructor below -- so T must be default-constructible to construct
	// a BoundedVector at all, not just to use the default/sized constructors or resize(). Asserted
	// here so that requirement is a clear, intentional message instead of a confusing compiler
	// error blaming whichever constructor happens to be instantiated first.
	static_assert(std::is_default_constructible_v<T>, "BoundedVector requires a default-constructible T");

public:
	constexpr BoundedVector() : components{}, count(0) {}

	// explicit, like std::vector(size_type): otherwise a bare int/short implicitly converts to
	// a whole BoundedVector wherever one is expected, which is surprising on its own and, worse,
	// can silently steal overload resolution from other constructors (e.g. a braced-init-list
	// of ints binding as "one BoundedVector per int" instead of "one BoundedVector holding these
	// ints" when inserting into a container of BoundedVector).
	explicit constexpr BoundedVector(short count) : count(count) {
		throw_if<std::length_error>(count > M, "BoundedVector overflow");
		for(short index = 0; index < count; ++index)
			components[index] = T{};
	}

	explicit constexpr BoundedVector(short count, T fillValue) : count(count) {
		throw_if<std::length_error>(count > M, "BoundedVector overflow");
		for(short index = 0; index < count; ++index)
			components[index] = fillValue;
	}

	constexpr BoundedVector(std::initializer_list<T> values) {
		reconfigure(std::begin(values), std::end(values));
	}

	// Updates
	template<class It>
	void reconfigure(It begin, It end) {
		count = 0;
		for(auto iterator = begin; iterator != end; ++iterator)
			push_back(*iterator);
	}

	constexpr inline void push_back(const T& item) {
		throw_if<std::length_error>(count >= M, "BoundedVector overflow");
		components[count++] = item;
	}

	// Constructs T from args (or
	// default-constructs it, given none), matching std::vector::emplace_back().
	template<class... Args>
	constexpr inline T& emplace_back(Args&&... args) {
		throw_if<std::length_error>(count >= M, "BoundedVector overflow");
		components[count] = T(std::forward<Args>(args)...);
		return components[count++];
	}

	constexpr inline void pop_back() {
		throw_if<std::out_of_range>(count == 0, "BoundedVector pop_back on empty vector");
		--count;
	}

	constexpr inline void clear() {
		count = 0;
	}

	// Grows or shrinks to newCount; elements added by growing are value-initialized, same as
	// std::vector::resize(). Shrinking, like erase()/pop_back(), just lowers count -- it doesn't
	// clear the now-unused tail.
	constexpr inline void resize(short newCount) {
		throw_if<std::length_error>(newCount > M, "BoundedVector overflow");
		for(short index = count; index < newCount; ++index)
			components[index] = T{};
		count = newCount;
	}

	// Takes item by value rather than by reference: the shift below moves the backing storage,
	// which would silently invalidate a reference into this same vector (e.g. insert(i,
	// vector[j])) before it gets read.
	constexpr inline void insert(unsigned int index, T item) {
		throw_if<std::out_of_range>(index > count, "BoundedVector insert index out of range");
		if(index == count)
			push_back(item);
		else {
			throw_if<std::length_error>(count >= M, "BoundedVector overflow");
			// memmove is a bulk byte copy -- only safe when T is trivially copyable. For
			// anything else (e.g. a type owning a resource), fall back to shifting one element
			// at a time via move-assignment, which respects T's real move semantics.
			if constexpr (std::is_trivially_copyable_v<T>)
				memmove(components + index + 1, components + index, (count - index) * sizeof(T));
			else
				for(unsigned int i = count; i > index; --i)
					components[i] = std::move(components[i - 1]);
			components[index] = std::move(item);
			count++;
		}
	}

	// Note for non-trivial T: like pop_back(), this only lowers count -- it doesn't destroy the
	// now-unreachable last element, so any resources it holds aren't released until that slot is
	// next overwritten (by push_back/insert/resize growing back into it) or the vector itself is
	// destroyed.
	constexpr inline void erase(unsigned int index) {
		throw_if<std::out_of_range>(index >= count, "BoundedVector erase index out of range");
		if(index != count - 1) {
			if constexpr (std::is_trivially_copyable_v<T>)
				memmove(components + index, components + index + 1, (count - index - 1) * sizeof(T));
			else
				for(unsigned int i = index; i < static_cast<unsigned int>(count) - 1; ++i)
					components[i] = std::move(components[i + 1]);
		}
		count--;
	}

	// Access
	constexpr T& operator[](unsigned int index) {
		return components[index];
	}
	constexpr const T& operator[](unsigned int index) const {
		return components[index];
	}
	constexpr T& at(unsigned int index) {
		throw_if<std::out_of_range>(index >= count, "BoundedVector at index out of range");
		return components[index];
	}
	constexpr const T& at(unsigned int index) const {
		throw_if<std::out_of_range>(index >= count, "BoundedVector at index out of range");
		return components[index];
	}
	const constexpr T* begin() const { return components; }
	const constexpr T* end() const { return components + count; }
	constexpr T* begin() { return components; }
	constexpr T* end() { return components + count; }
	const constexpr T* data() const { return components; }
	constexpr T* data() { return components; }
	constexpr bool empty() const { return count == 0; }
	constexpr short size() const { return count; }
	constexpr short capacity() const { return M; }
	constexpr const T& front() const { return components[0]; }
	constexpr T& front() { return components[0]; }
	constexpr const T& back() const {
		return components[count - 1];
	}
	constexpr T& back() {
		return components[count - 1];
	}

	struct Hash {
		size_t operator()(const BoundedVector& other) const {
			size_t hash = 0;
			for(short index = 0; index < other.count; ++index)
				hash += 5 * hash + other.components[index];
			return hash;
		}
	};

	struct Equal {
		bool operator()(const BoundedVector& lhs, const BoundedVector& rhs) const {
			if(lhs.count != rhs.count)
				return false;
			for(short index = 0; index < lhs.count; ++index)
				if(!(lhs.components[index] == rhs.components[index]))
					return false;
			return true;
		}
	};

	constexpr bool operator==(const BoundedVector& other) const { return Equal{}(*this, other); }
	constexpr bool operator!=(const BoundedVector& other) const { return !(*this == other); }

private:
	T components[M];
	short count = 0;
};
