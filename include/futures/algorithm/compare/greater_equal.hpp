//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_ALGORITHM_COMPARE_GREATER_EQUAL_HPP
#define FUTURES_ALGORITHM_COMPARE_GREATER_EQUAL_HPP

/**
 *  @file algorithm/compare/greater_equal.hpp
 *  @brief Greater or equal comparison functor
 *
 *  This file defines the greater or equal operator as a functor.
 */

#include <futures/algorithm/compare/less.hpp>
#include <utility>
#include <type_traits>

namespace futures {
    /// A C++17 functor equivalent to the C++20 std::ranges::greater_equal
    /**
     * @see [`std::greater_equal`](https://en.cppreference.com/w/cpp/utility/functional/greater_equal)
     */
    struct greater_equal {
        FUTURES_TEMPLATE(class T, class U)
        (requires is_totally_ordered_with_v<T, U>) constexpr bool
        operator()(T &&t, U &&u) const {
            return !less{}(std::forward<T>(t), std::forward<U>(u));
        }
        using is_transparent = void;
    };

} // namespace futures

#endif // FUTURES_ALGORITHM_COMPARE_GREATER_EQUAL_HPP
