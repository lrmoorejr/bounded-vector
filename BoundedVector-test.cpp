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

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "BoundedVector.hpp"

TEST_CASE( "Default construction is empty" ) {
	BoundedVector<int, 4> vector;
	CHECK(vector.empty());
	CHECK(vector.size() == 0);
	CHECK(vector.capacity() == 4);
}

TEST_CASE( "Default construction works for a non-int-constructible trivially copyable T" ) {
	struct Point { float x, y; };
	BoundedVector<Point, 4> vector;
	CHECK(vector.size() == 0);
}

TEST_CASE( "data() matches begin()" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	CHECK(vector.data() == vector.begin());
	vector.data()[0] = 10;
	CHECK(vector[0] == 10);
}

TEST_CASE( "Sized constructor value-initializes exactly size elements" ) {
	BoundedVector<int, 4> vector(3);
	CHECK(vector.size() == 3);
	for(int value : vector)
		CHECK(value == 0);
}

TEST_CASE( "Fill constructor fills every element, not just the first" ) {
	BoundedVector<int, 5> vector(4, 7);
	CHECK(vector.size() == 4);
	for(int value : vector)
		CHECK(value == 7);
}

TEST_CASE( "push_back, insert, erase, clear" ) {
	BoundedVector<int, 4> vector;
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	CHECK(vector.size() == 3);

	vector.insert(1, 99);
	CHECK(vector.size() == 4);
	CHECK(vector[0] == 1);
	CHECK(vector[1] == 99);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);

	vector.erase(0);
	CHECK(vector.size() == 3);
	CHECK(vector[0] == 99);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 3);

	vector.clear();
	CHECK(vector.empty());
}

TEST_CASE( "insert at the end behaves like push_back" ) {
	BoundedVector<int, 4> vector = {1, 2};
	vector.insert(2, 3);
	CHECK(vector.size() == 3);
	CHECK(vector[2] == 3);
}

TEST_CASE( "insert survives its source aliasing the vector itself" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	vector.insert(1, vector[2]);
	CHECK(vector.size() == 4);
	CHECK(vector[0] == 1);
	CHECK(vector[1] == 3);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);
}

TEST_CASE( "erase at the last index doesn't need to shift anything" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	vector.erase(2);
	CHECK(vector.size() == 2);
	CHECK(vector[0] == 1);
	CHECK(vector[1] == 2);
}

TEST_CASE( "emplace_back constructs T from its arguments" ) {
	BoundedVector<int, 4> vector;
	vector.emplace_back(42);
	CHECK(vector.size() == 1);
	CHECK(vector[0] == 42);

	vector.emplace_back();
	CHECK(vector.size() == 2);
	CHECK(vector[1] == 0);
}

TEST_CASE( "pop_back shrinks by one" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	vector.pop_back();
	CHECK(vector.size() == 2);
	CHECK(vector.back() == 2);
}

TEST_CASE( "resize grows with value-initialized elements and shrinks in place" ) {
	BoundedVector<int, 5> vector = {1, 2};
	vector.resize(4);
	CHECK(vector.size() == 4);
	CHECK(vector[0] == 1);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 0);
	CHECK(vector[3] == 0);

	vector.resize(1);
	CHECK(vector.size() == 1);
	CHECK(vector[0] == 1);
}

TEST_CASE( "front and back" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	CHECK(vector.front() == 1);
	CHECK(vector.back() == 3);
	vector.front() = 10;
	CHECK(vector[0] == 10);
}

TEST_CASE( "at() matches operator[] for in-range access" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	CHECK(vector.at(1) == vector[1]);
	vector.at(1) = 20;
	CHECK(vector[1] == 20);
}

TEST_CASE( "const accessors" ) {
	const BoundedVector<int, 4> vector = {1, 2, 3};
	CHECK(vector[1] == 2);
	CHECK(vector.at(1) == 2);
	CHECK(vector.front() == 1);
	CHECK(vector.back() == 3);
	CHECK(*vector.data() == 1);
	CHECK(*vector.begin() == 1);
	CHECK(vector.end() == vector.begin() + vector.size());
}

TEST_CASE( "copy construction and assignment are independent" ) {
	BoundedVector<int, 4> original = {1, 2, 3};

	BoundedVector<int, 4> copy = original;
	copy[0] = 99;
	CHECK(original[0] == 1);
	CHECK(copy[0] == 99);

	BoundedVector<int, 4> assigned;
	assigned = original;
	assigned.push_back(4);
	CHECK(original.size() == 3);
	CHECK(assigned.size() == 4);
}

TEST_CASE( "filling to exactly capacity succeeds" ) {
	BoundedVector<int, 3> vector;
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	CHECK(vector.size() == vector.capacity());
	CHECK(vector.back() == 3);
}

TEST_CASE( "reconfigure replaces existing contents rather than appending" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	int replacement[] = {9, 8};
	vector.reconfigure(std::begin(replacement), std::end(replacement));
	CHECK(vector.size() == 2);
	CHECK(vector[0] == 9);
	CHECK(vector[1] == 8);
}

TEST_CASE( "Hash and Equal work as a real std::unordered_set key" ) {
	using Key = BoundedVector<int, 4>;
	std::unordered_set<Key, Key::Hash, Key::Equal> set;
	set.insert({1, 2});
	set.insert({1, 2}); // duplicate, should not grow the set
	set.insert({1, 3});

	CHECK(set.size() == 2);
	CHECK(set.count(Key{1, 2}) == 1);
	CHECK(set.count(Key{1, 3}) == 1);
	CHECK(set.count(Key{4, 5}) == 0);
}

TEST_CASE( "operator== and operator!= compare only the logical elements" ) {
	BoundedVector<int, 4> a = {1, 2};
	BoundedVector<int, 4> b;
	b.push_back(1);
	b.push_back(2);
	b.push_back(9);
	b.pop_back();

	CHECK(a == b);
	CHECK_FALSE(a != b);

	BoundedVector<int, 4> c = {1, 3};
	CHECK(a != c);
}

TEST_CASE( "Hash and Equal agree for vectors with the same logical contents" ) {
	// Built two different ways so their unused tail elements diverge -- Equal must still
	// consider them equal since it only compares the first `size()` elements.
	BoundedVector<int, 4> a(2, 5);
	BoundedVector<int, 4> b;
	b.push_back(5);
	b.push_back(5);
	b.insert(0, 9);
	b.erase(0);

	BoundedVector<int, 4>::Equal equal;
	BoundedVector<int, 4>::Hash hash;
	CHECK(equal(a, b));
	CHECK(hash(a) == hash(b));
}

TEST_CASE( "Equal distinguishes different sizes and different contents" ) {
	BoundedVector<int, 4>::Equal equal;
	BoundedVector<int, 4> a = {1, 2};
	BoundedVector<int, 4> b = {1, 2, 3};
	BoundedVector<int, 4> c = {1, 3};
	CHECK_FALSE(equal(a, b));
	CHECK_FALSE(equal(a, c));
}

TEST_CASE( "initializer_list construction" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};
	CHECK(vector.size() == 3);
	CHECK(vector[0] == 1);
	CHECK(vector[2] == 3);
}

TEST_CASE( "insert and erase move-shift a non-trivially-copyable T correctly" ) {
	BoundedVector<std::string, 4> vector;
	vector.push_back("a");
	vector.push_back("b");
	vector.push_back("c");

	vector.insert(1, std::string("x"));
	CHECK(vector.size() == 4);
	CHECK(vector[0] == "a");
	CHECK(vector[1] == "x");
	CHECK(vector[2] == "b");
	CHECK(vector[3] == "c");

	vector.erase(0);
	CHECK(vector.size() == 3);
	CHECK(vector[0] == "x");
	CHECK(vector[1] == "b");
	CHECK(vector[2] == "c");
}

TEST_CASE( "insert survives self-aliasing for a non-trivially-copyable T too" ) {
	BoundedVector<std::string, 4> vector;
	vector.push_back("a");
	vector.push_back("b");
	vector.push_back("c");

	vector.insert(1, vector[2]);
	CHECK(vector.size() == 4);
	CHECK(vector[0] == "a");
	CHECK(vector[1] == "c");
	CHECK(vector[2] == "b");
	CHECK(vector[3] == "c");
}

TEST_CASE( "Exceeding capacity throws std::length_error" ) {
	CHECK_THROWS_AS((BoundedVector<int, 4>(5)), std::length_error);
	CHECK_THROWS_AS((BoundedVector<int, 4>(5, 0)), std::length_error);

	BoundedVector<int, 2> vector = {1, 2};
	CHECK_THROWS_AS(vector.push_back(3), std::length_error);
	CHECK_THROWS_AS(vector.emplace_back(3), std::length_error);
	CHECK_THROWS_AS(vector.resize(3), std::length_error);
	CHECK_THROWS_AS(vector.insert(0, 3), std::length_error);

	// The vector is untouched by a failed mutation -- these all throw before touching count.
	CHECK(vector.size() == 2);
}

TEST_CASE( "Out-of-range access and empty-vector removal throw std::out_of_range" ) {
	BoundedVector<int, 4> vector = {1, 2, 3};

	CHECK_THROWS_AS(vector.at(3), std::out_of_range);
	CHECK_THROWS_AS(std::as_const(vector).at(3), std::out_of_range);
	CHECK_THROWS_AS(vector.insert(4, 99), std::out_of_range);
	CHECK_THROWS_AS(vector.erase(3), std::out_of_range);

	BoundedVector<int, 4> empty;
	CHECK_THROWS_AS(empty.pop_back(), std::out_of_range);
}

TEST_CASE( "BoundedVector vs std::vector: construct/fill/destroy churn" ) {
	// The scenario BoundedVector is for: a small, short-lived vector built and torn down at high
	// frequency. std::vector pays a heap allocation (and, without reserve(), possibly more than
	// one as it grows) every time through the loop; BoundedVector never allocates at all.
	//
	// The capacity itself (4) has to be a real compile-time constant -- BoundedVector's whole
	// design point is a fixed capacity known at compile time. But the *loop bound* is read
	// through a volatile so the compiler can't see "always 4 iterations, sum is always 6" and
	// fold the entire benchmark -- allocation included -- down to a constant.
	constexpr int Capacity = 4;
	volatile int n = Capacity;

	BENCHMARK("std::vector<int>, no reserve") {
		std::vector<int> v;
		for(int i = 0; i < n; ++i)
			v.push_back(i);
		int sum = 0;
		for(int x : v)
			sum += x;
		return sum;
	};

	BENCHMARK("std::vector<int>, reserved up front") {
		std::vector<int> v;
		v.reserve(n);
		for(int i = 0; i < n; ++i)
			v.push_back(i);
		int sum = 0;
		for(int x : v)
			sum += x;
		return sum;
	};

	BENCHMARK("BoundedVector<int, 4>") {
		BoundedVector<int, Capacity> v;
		for(int i = 0; i < n; ++i)
			v.push_back(i);
		int sum = 0;
		for(int x : v)
			sum += x;
		return sum;
	};
}
