//
// Copyright (c) 2021 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_IS_WHEN_ANY_RESULT_H
#define FUTURES_IS_WHEN_ANY_RESULT_H

#include <futures/adaptor/when_any_result.h>
#include <type_traits>

namespace futures::detail {
    /** \addtogroup futures Futures
     *  @{
     */

    /// Check if type is a when_any_result
    template <typename>
    struct is_when_any_result : std::false_type
    {};
    template <typename Sequence>
    struct is_when_any_result<when_any_result<Sequence>> : std::true_type
    {};
    template <typename Sequence>
    struct is_when_any_result<const when_any_result<Sequence>> : std::true_type
    {};
    template <typename Sequence>
    struct is_when_any_result<when_any_result<Sequence> &> : std::true_type
    {};
    template <typename Sequence>
    struct is_when_any_result<when_any_result<Sequence> &&> : std::true_type
    {};
    template <typename Sequence>
    struct is_when_any_result<const when_any_result<Sequence> &>
        : std::true_type
    {};
    template <class T>
    constexpr bool is_when_any_result_v = is_when_any_result<T>::value;

    /** @} */
} // namespace futures::detail


#endif // FUTURES_IS_WHEN_ANY_RESULT_H
