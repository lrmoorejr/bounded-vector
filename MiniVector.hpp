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
#include <type_traits>
#include <utility>

// Ensure.hpp is an optional dependency: if it's available (either as part of this
// checkout or vendored alongside this header), use its ensure() for a formatted
// diagnostic on failure; otherwise fall back to plain assert() so this header still
// works standalone.
#if __has_include("commons/Ensure.hpp")
	#include "commons/Ensure.hpp"
#elif __has_include("Ensure.hpp")
	#include "Ensure.hpp"
#else
	#include <cassert>
	// Guard against Ensure.hpp having already been included under a path our
	// __has_include checks above don't know about (e.g. vendored elsewhere as
	// "3rdparty/Ensure.hpp") -- COMMONS_ENSURE_HPP is defined by Ensure.hpp
	// itself, so this still catches that case even under an unknown filename.
	#if !defined(COMMONS_ENSURE_HPP) && !defined(ensure)
		#define ensure(condition, ...) assert((condition))
	#endif
#endif

template<typename T, short M>
class MiniVector {
public:
	constexpr MiniVector() : components{}, count(0) {}

	// explicit, like std::vector(size_type): otherwise a bare int/short implicitly converts to
	// a whole MiniVector wherever one is expected, which is surprising on its own and, worse,
	// can silently steal overload resolution from other constructors (e.g. a braced-init-list
	// of ints binding as "one MiniVector per int" instead of "one MiniVector holding these
	// ints" when inserting into a container of MiniVector).
	explicit constexpr MiniVector(short count) : count(count) {
		ensure(count <= M, "MiniVector overflow");
		for(short index = 0; index < count; ++index)
			components[index] = T{};
	}

	explicit constexpr MiniVector(short count, T fillValue) : count(count) {
		ensure(count <= M, "MiniVector overflow");
		for(short index = 0; index < count; ++index)
			components[index] = fillValue;
	}

	constexpr MiniVector(std::initializer_list<T> values) {
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
		ensure(count < M, "MiniVector overflow");
		components[count++] = item;
	}

	// Constructs T from args (or
	// default-constructs it, given none), matching std::vector::emplace_back().
	template<class... Args>
	constexpr inline T& emplace_back(Args&&... args) {
		ensure(count < M, "MiniVector overflow");
		components[count] = T(std::forward<Args>(args)...);
		return components[count++];
	}

	constexpr inline void pop_back() {
		ensure(count > 0, "MiniVector pop_back on empty vector");
		--count;
	}

	constexpr inline void clear() {
		count = 0;
	}

	// Grows or shrinks to newCount; elements added by growing are value-initialized, same as
	// std::vector::resize(). Shrinking, like erase()/pop_back(), just lowers count -- it doesn't
	// clear the now-unused tail.
	constexpr inline void resize(short newCount) {
		ensure(newCount <= M, "MiniVector overflow");
		for(short index = count; index < newCount; ++index)
			components[index] = T{};
		count = newCount;
	}

	// Takes item by value rather than by reference: the shift below moves the backing storage,
	// which would silently invalidate a reference into this same vector (e.g. insert(i,
	// vector[j])) before it gets read.
	constexpr inline void insert(unsigned int index, T item) {
		ensure(index <= count, "MiniVector insert index out of range");
		if(index == count)
			push_back(item);
		else {
			ensure(count < M, "MiniVector overflow");
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
		ensure(index < count, "MiniVector erase index out of range");
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
		ensure(index < count, "MiniVector at index out of range");
		return components[index];
	}
	constexpr const T& at(unsigned int index) const {
		ensure(index < count, "MiniVector at index out of range");
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
		size_t operator()(const MiniVector& other) const {
			size_t hash = 0;
			for(short index = 0; index < other.count; ++index)
				hash += 5 * hash + other.components[index];
			return hash;
		}
	};

	struct Equal {
		bool operator()(const MiniVector& lhs, const MiniVector& rhs) const {
			if(lhs.count != rhs.count)
				return false;
			for(short index = 0; index < lhs.count; ++index)
				if(!(lhs.components[index] == rhs.components[index]))
					return false;
			return true;
		}
	};

	constexpr bool operator==(const MiniVector& other) const { return Equal{}(*this, other); }
	constexpr bool operator!=(const MiniVector& other) const { return !(*this == other); }

private:
	T components[M];
	short count = 0;
};
