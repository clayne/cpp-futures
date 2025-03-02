//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_ALGORITHM_TRAITS_IS_INPUT_RANGE_HPP
#define FUTURES_ALGORITHM_TRAITS_IS_INPUT_RANGE_HPP

/**
 *  @file algorithm/traits/is_input_range.hpp
 *  @brief `is_input_range` trait
 *
 *  This file defines the `is_input_range` trait.
 */

#include <futures/algorithm/traits/is_input_iterator.hpp>
#include <futures/algorithm/traits/is_range.hpp>
#include <futures/algorithm/traits/iterator.hpp>
#include <futures/detail/traits/std_type_traits.hpp>
#include <type_traits>

namespace futures {
    /** @addtogroup algorithms Algorithms
     *  @{
     */

    /** @addtogroup traits Traits
     *  @{
     */

    /// @brief A type trait equivalent to the `std::input_range` concept
    /**
     * @see [`std::ranges::input_range`](https://en.cppreference.com/w/cpp/ranges/input_range)
     */
#ifdef FUTURES_DOXYGEN
    template <class T>
    using is_input_range = std::bool_constant<std::is_input_range<T>>;
#else
    template <class T, class = void>
    struct is_input_range : std::false_type {};

    template <class T>
    struct is_input_range<T, detail::void_t<iterator_t<T>>>
        : detail::conjunction<
              // clang-format off
              is_range<T>,
              is_input_iterator<iterator_t<T>>
              // clang-format off
            >
    {};
#endif

    /// @copydoc is_input_range
    template <class T>
    bool constexpr is_input_range_v = is_input_range<T>::value;

    /** @} */
    /** @} */
} // namespace futures

#endif // FUTURES_ALGORITHM_TRAITS_IS_INPUT_RANGE_HPP
