/*
 Copyright 2020 Kristian Duske

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <catch2/catch.hpp>

#include "GTestCompat.h"

#include "kdl/overload.h"
#include "kdl/result.h"
#include "kdl/result_combine.h"
#include "kdl/result_io.h"

#include <iostream>
#include <string>

namespace kdl {
    struct Error1 {};
    struct Error2 {};
    struct Error3 {};
    
    inline bool operator==(const Error1&, const Error1&) {
        return true;
    }

    inline bool operator==(const Error2&, const Error2&) {
        return true;
    }

    inline bool operator==(const Error3&, const Error3&) {
        return true;
    }

    inline std::ostream& operator<<(std::ostream& str, const Error1&) {
        str << "Error1";
        return str;
    }

    inline std::ostream& operator<<(std::ostream& str, const Error2&) {
        str << "Error2";
        return str;
    }

    inline std::ostream& operator<<(std::ostream& str, const Error3&) {
        str << "Error3";
        return str;
    }

    struct Counter {
        std::size_t copies = 0u;
        std::size_t moves = 0u;
        
        Counter() = default;
        
        Counter(const Counter& c)
        : copies(c.copies + 1u)
        , moves(c.moves) {}
        
        Counter(Counter&& c)
        : copies(c.copies)
        , moves(c.moves + 1u) {}

        Counter& operator=(const Counter& c) {
            this->copies = c.copies + 1u;
            this->moves = c.moves;
            return *this;
        }
        
        Counter& operator=(Counter&& c) {
            this->copies = c.copies;
            this->moves = c.moves + 1u;
            return *this;
        }
    };

    inline std::ostream& operator<<(std::ostream& str, const Counter&) {
        str << "Counter";
        return str;
    }
        
    /**
     * Tests construction of a successful result.
     */
    template <typename ResultType, typename... V>
    void test_construct_success(V&&... v) {
        auto result = ResultType::success(std::forward<V>(v)...);
        ASSERT_TRUE(result.is_success());
        ASSERT_FALSE(result.is_error());
        ASSERT_EQ(result.is_success(), result);
    }
    
    /**
     * Tests construction of an error result.
     */
    template <typename ResultType, typename E>
    void test_construct_error(E&& e) {
        auto result = ResultType::error(std::forward<E>(e));
        ASSERT_FALSE(result.is_success());
        ASSERT_TRUE(result.is_error());
        ASSERT_EQ(result.is_success(), result);
    }
    
    /**
     * Tests visiting a successful result and passing by const lvalue reference to the visitor.
     */
    template <typename ResultType, typename V>
    void test_visit_success_const_lvalue_ref(V&& v) {
        auto result = ResultType::success(std::forward<V>(v));

        ASSERT_TRUE(result.visit(overload(
            [&] (const auto& x) { return x == v; },
            []  (const Error1&) { return false; },
            []  (const Error2&) { return false; }
        )));
    }
    
    /**
     * Tests visiting a successful result and passing by rvalue reference to the visitor.
     */
    template <typename ResultType, typename V>
    void test_visit_success_rvalue_ref(V&& v) {
        auto result = ResultType::success(std::forward<V>(v));

        ASSERT_TRUE(std::move(result).visit(overload(
            [&] (auto&&)   { return true; },
            []  (Error1&&) { return false; },
            []  (Error2&&) { return false; }
        )));
        
        typename ResultType::value_type y;
        std::move(result).visit(overload(
            [&] (auto&& x) { y = std::move(x); },
            []  (Error1&&) {},
            []  (Error2&&) {}
        ));
        
        ASSERT_EQ(0u, y.copies);
    }

    /**
     * Tests visiting an error result and passing by const lvalue reference to the visitor.
     */
    template <typename ResultType, typename E>
    void test_visit_error_const_lvalue_ref(E&& e) {
        auto result = ResultType::error(std::forward<E>(e));

        ASSERT_TRUE(result.visit(overload(
            []  (const auto&)   { return false; },
            [&] (const E& x)    { return x == e; },
            []  (const Error2&) { return false; }
        )));
    }
    
    /**
     * Tests visiting an error result and passing by rvalue reference to the visitor.
     */
    template <typename ResultType, typename E>
    void test_visit_error_rvalue_ref(E&& e) {
        auto result = ResultType::error(std::forward<E>(e));

        ASSERT_TRUE(std::move(result).visit(overload(
            []  (auto&&)   { return false; },
            [&] (E&&)      { return true; },
            []  (Error2&&) { return false; }
        )));
        
        E y;
        std::move(result).visit(overload(
            []  (auto&&)   {},
            [&] (E&& x)    { y = std::move(x); },
            []  (Error2&&) {}
        ));
        
        ASSERT_EQ(0u, y.copies);
    }
    
    /**
     * Tests mapping a successful result and passing by const lvalue reference to the mapping function.
     */
    template <typename FromResult, typename ToValueType, typename V>
    void test_and_then_const_lvalue_ref(V&& v) {
        auto from = FromResult::success(std::forward<V>(v));
        
        const auto to = from.and_then([](const typename FromResult::value_type& x) {
            return kdl::result<ToValueType, Error3>::success(static_cast<ToValueType>(x));
        });
        ASSERT_TRUE(to.is_success());
        ASSERT_FALSE(to.is_error());
        ASSERT_EQ(to.is_success(), to);

        ASSERT_TRUE(to.visit(overload(
            [](const ToValueType&) { return true; },
            [](const auto&) { return false; }
        )));
    }
    
    /**
     * Tests mapping a successful result and passing by rvalue reference to the mapping function.
     */
    template <typename FromResult, typename ToValueType, typename V>
    void test_and_then_rvalue_ref(V&& v) {
        auto from = FromResult::success(std::forward<V>(v));
        const auto to = std::move(from).and_then([](typename FromResult::value_type&& x) {
            return kdl::result<ToValueType, Error3>::success(std::move(static_cast<ToValueType>(x)));
        });
        ASSERT_TRUE(to.is_success());
        ASSERT_FALSE(to.is_error());
        ASSERT_EQ(to.is_success(), to);

        ASSERT_TRUE(to.visit(overload(
            [](const ToValueType&) { return true; },
            [](const auto&) { return false; }
        )));

        ToValueType y;
        std::move(to).visit(overload(
            [&] (ToValueType&& x) { y = x; },
            [] (auto&&) {}
        ));
        
        ASSERT_EQ(0u, y.copies);
    }
    
    /**
     * Tests visiting a successful result when there is no value.
     */
    template <typename ResultType>
    void test_visit_success_with_opt_value() {
        auto result = ResultType::success();
        
        ASSERT_TRUE(result.visit(overload(
            []()            { return true; },
            [](const auto&) { return false; }
        )));
    }

    /**
     * Tests visiting a successful result and passing by const lvalue reference to the visitor
     * when the value is optional (or void).
     */
    template <typename ResultType, typename V>
    void test_visit_success_const_lvalue_ref_with_opt_value(V&& v) {
        auto result = ResultType::success(std::forward<V>(v));
        
        ASSERT_TRUE(result.visit(overload(
            []  ()              { return false; },
            [&] (const auto& x) { return x == v; },
            []  (const Error1&) { return false; },
            []  (const Error2&) { return false; }
        )));
    }

    /**
     * Tests visiting a successful result and passing by rvalue reference to the visitor
     * when the value is optional (or void).
     */
    template <typename ResultType, typename V>
    void test_visit_success_rvalue_ref_with_opt_value(V&& v) {
        auto result = ResultType::success(std::forward<V>(v));

        ASSERT_TRUE(std::move(result).visit(overload(
            []  ()         { return false; },
            [&] (auto&&)   { return true; },
            []  (Error1&&) { return false; },
            []  (Error2&&) { return false; }
        )));
        
        typename ResultType::value_type y;
        std::move(result).visit(overload(
            [] ()         {},
            [&] (auto&& x) { y = std::move(x); },
            []  (Error1&&) {},
            []  (Error2&&) {}
        ));
        
        ASSERT_EQ(0u, y.copies);
    }

    /**
     * Tests visiting an error result and passing by const lvalue reference to the visitor
     * when the value is optional (or void).
     */
    template <typename ResultType, typename E>
    void test_visit_error_const_lvalue_ref_with_opt_value(E&& e) {
        auto result = ResultType::error(std::forward<E>(e));
        
        ASSERT_TRUE(result.visit(overload(
            []  ()            { return false; },
            []  (const auto&) { return false; },
            [&] (const E& x)  { return x == e; }
        )));
    }
    
    /**
     * Tests visiting an error result and passing by rvalue reference to the visitor
     * when the value is optional (or void).
     */
    template <typename ResultType, typename E>
    void test_visit_error_rvalue_ref_with_opt_value(E&& e) {
        auto result = ResultType::error(std::forward<E>(e));

        ASSERT_TRUE(std::move(result).visit(overload(
            [] ()       { return false; },
            []  (auto&&) { return false; },
            [&] (E&&)    { return true; }
        )));

        E y;
        std::move(result).visit(overload(
            []  ()       {},
            []  (auto&&) {},
            [&] (E&& x)  { y = std::move(x); }
        ));
        
        ASSERT_EQ(0u, y.copies);
    }

    TEST_CASE("result_test.void_result", "[result_test]") {
        kdl::result<void, Error1> r1 = result<int, Error1>::success(1).and_then(
            [](int) {
                return void_result;
            }
        );
        
        CHECK(r1.success());
    }

    TEST_CASE("result_test.constructor", "[result_test]") {
        ASSERT_TRUE((result<int, float, std::string>::success(1).is_success()));
        ASSERT_TRUE((result<int, float, std::string>::error(1.0f).is_error()));
        ASSERT_TRUE((result<int, float, std::string>::error("").is_error()));

        test_construct_success<const result<int, Error1, Error2>>(1);
        test_construct_success<result<int, Error1, Error2>>(1);
        test_construct_success<const result<const int, Error1, Error2>>(1);
        test_construct_success<result<const int, Error1, Error2>>(1);

        test_construct_error<const result<int, Error1, Error2>>(Error1{});
        test_construct_error<result<int, Error1, Error2>>(Error1{});
        test_construct_error<const result<const int, Error1, Error2>>(Error1{});
        test_construct_error<result<const int, Error1, Error2>>(Error1{});

        test_construct_error<const result<int, Error1, Error2>>(Error2{});
        test_construct_error<result<int, Error1, Error2>>(Error2{});
        test_construct_error<const result<const int, Error1, Error2>>(Error2{});
        test_construct_error<result<const int, Error1, Error2>>(Error2{});
    }

    TEST_CASE("result_test.visit", "[result_test]") {
        test_visit_success_const_lvalue_ref<const result<int, Error1, Error2>>(1);
        test_visit_success_const_lvalue_ref<result<int, Error1, Error2>>(1);
        test_visit_success_const_lvalue_ref<const result<const int, Error1, Error2>>(1);
        test_visit_success_const_lvalue_ref<result<const int, Error1, Error2>>(1);
        test_visit_success_rvalue_ref<result<Counter, Error1, Error2>>(Counter{});

        test_visit_error_const_lvalue_ref<const result<int, Error1, Error2>>(Error1{});
        test_visit_error_const_lvalue_ref<result<int, Error1, Error2>>(Error1{});
        test_visit_error_const_lvalue_ref<const result<const int, Error1, Error2>>(Error1{});
        test_visit_error_const_lvalue_ref<result<const int, Error1, Error2>>(Error1{});
        test_visit_error_rvalue_ref<result<int, Counter, Error2>>(Counter{});
    }

    TEST_CASE("result_test.and_then", "[result_test]") {
        test_and_then_const_lvalue_ref<const result<int, Error1, Error2>, float>(1);
        test_and_then_const_lvalue_ref<result<int, Error1, Error2>, float>(1);
        test_and_then_const_lvalue_ref<const result<const int, Error1, Error2>, float>(1);
        test_and_then_const_lvalue_ref<result<const int, Error1, Error2>, float>(1);
        test_and_then_rvalue_ref<result<Counter, Error1, Error2>, Counter>(Counter{});
    }
    
    TEST_CASE("void_result_test.constructor", "[void_result_test]") {
        ASSERT_TRUE((result<void, float, std::string>::success().is_success()));
        ASSERT_TRUE((result<void, float, std::string>::error(1.0f).is_error()));
        ASSERT_TRUE((result<void, float, std::string>::error("").is_error()));

        test_construct_success<const result<void, Error1, Error2>>();
        test_construct_success<result<void, Error1, Error2>>();

        test_construct_error<const result<void, Error1, Error2>>(Error1{});
        test_construct_error<result<void, Error1, Error2>>(Error1{});

        test_construct_error<const result<void, Error1, Error2>>(Error2{});
        test_construct_error<result<void, Error1, Error2>>(Error2{});
    }

    TEST_CASE("void_result_test.visit", "[void_result_test]") {
        test_visit_success_with_opt_value<const result<void, Error1, Error2>>();
        test_visit_success_with_opt_value<result<void, Error1, Error2>>();
        
        test_visit_error_const_lvalue_ref_with_opt_value<const result<void, Error1, Error2>>(Error1{});
        test_visit_error_const_lvalue_ref_with_opt_value<result<void, Error1, Error2>>(Error1{});
        test_visit_error_rvalue_ref_with_opt_value<result<void, Counter, Error2>>(Counter{});
    }
    
    TEST_CASE("void_result_test.and_then", "[void_result_test]") {
        const auto r_success = result<void, Error1, Error2>::success();
        const auto r_error   = result<void, Error1, Error2>::error(Error2{});
        
        const auto f_success = []() { return kdl::result<bool, Error3>::success(true); };
        const auto f_error   = []() { return kdl::result<bool, Error3>::error(Error3{}); };
        const auto f_void    = []() { return kdl::result<void, Error3>::success(); };

        CHECK(r_success.and_then(f_success) == result<bool, Error1, Error2, Error3>::success(true));
        CHECK(r_success.and_then(f_error)   == result<bool, Error1, Error2, Error3>::error(Error3{}));
        CHECK(r_error.and_then(f_success)   == result<bool, Error1, Error2, Error3>::error(Error2{}));
        CHECK(r_success.and_then(f_void)    == result<void, Error1, Error2, Error3>::success());
    }
    
    TEST_CASE("combine_results", "result_test") {
        using R1 = result<int, Error1, Error2>;
        using R2 = result<double, Error2, Error3>;
        
        auto r1 = R1::success(1);
        auto r2 = R2::success(2.0);
        auto r3 = R2::error(Error2{});
        
        CHECK(combine_results(r1, r2).is_success());
        CHECK(combine_results(r1, r2).visit(kdl::overload(
            [](const std::tuple<int, double>& t) {
                CHECK(t == std::make_tuple(1, 2.0));
                return true;
            },
            [](const auto&) {
                return false;
            }
        )));
        
        CHECK(combine_results(r1, r3).is_error());
        CHECK(combine_results(r1, r3).visit(kdl::overload(
            [](const std::tuple<int, double>&) {
                return false;
            },
            [](const Error2&) {
                return true;
            },
            [](const auto&) {
                return false;
            }
        )));

        CHECK(combine_results(r1, R2::success(2.0)).is_success());
        CHECK(combine_results(r1, R2::success(2.0)).visit(kdl::overload(
            [](const std::tuple<int, double>& t) {
                CHECK(t == std::make_tuple(1, 2.0));
                return true;
            },
            [](const auto&) {
                return false;
            }
        )));
        
        CHECK(combine_results(r1, R2::error(Error2{})).is_error());
        CHECK(combine_results(r1, R2::error(Error2{})).visit(kdl::overload(
            [](const std::tuple<int, double>&) {
                return false;
            },
            [](const Error2&) {
                return true;
            },
            [](const auto&) {
                return false;
            }
        )));
    }
}
