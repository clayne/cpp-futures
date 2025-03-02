//
// Copyright (c) 2021 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_EXECUTOR_DETAIL_IS_EXECUTOR_HPP
#define FUTURES_EXECUTOR_DETAIL_IS_EXECUTOR_HPP

#include <futures/config.hpp>
#include <futures/algorithm/traits/is_equality_comparable.hpp>
#include <futures/detail/traits/std_type_traits.hpp>

namespace futures {
    namespace detail {
        struct invocable_archetype {
            void
            operator()() const {}
        };

        // Check if a type implements the get_executor() function
        template <class T, typename = void>
        struct has_get_executor : std::false_type {};

        template <class T>
        struct has_get_executor<
            T,
            void_t<decltype(std::declval<T>().get_executor())>>
            : std::true_type {};

        // Check if a type implements the get function
        template <class T, class F = invocable_archetype, typename = void>
        struct has_execute : std::false_type {};

        template <class T, class F>
        struct has_execute<
            T,
            F,
            void_t<decltype(std::declval<T>().execute(std::declval<F>()))>>
            : std::true_type {};

        // Check if a type implements the get function but not the get_execute
        // This is a workaround for GCC7, which fails to return false for
        // has_execute when the class has a private execute function
        template <class T, class F = invocable_archetype, class = void>
        struct is_light_executor_impl : std::false_type {};

        template <class T, class F>
        struct is_light_executor_impl<
            T,
            F,
            std::enable_if_t<
                // clang-format off
                !detail::has_get_executor<T>::value
                // clang-format on
                >> : has_execute<T, F> {};

        // check if something is an asio executor
        template <class T, class F = invocable_archetype, class = void>
        struct is_asio_executor_for_impl : std::false_type {};

        template <class T, class F>
        struct is_asio_executor_for_impl<
            T,
            F,
            void_t<
                // clang-format off
                decltype(std::declval<T>().context()),
                decltype(std::declval<T>().on_work_started()),
                decltype(std::declval<T>().on_work_finished()),
                decltype(std::declval<T>().dispatch(std::declval<F>(), std::declval<std::allocator<void>>())),
                decltype(std::declval<T>().post(std::declval<F>(), std::declval<std::allocator<void>>())),
                decltype(std::declval<T>().defer(std::declval<F>(), std::declval<std::allocator<void>>()))
                // clang-format on
                >>
            : detail::conjunction<
                  std::is_copy_constructible<T>,
                  is_equality_comparable<T>> {};

        template <class T, class F = invocable_archetype>
        struct is_asio_executor_for : is_asio_executor_for_impl<T, F> {};

        // Check if a type implements the get function
        template <class T, class F = invocable_archetype>
        using is_executor_for_impl = conjunction<
            is_light_executor_impl<T, F>,
            std::is_copy_constructible<T>>;

    } // namespace detail
} // namespace futures

#endif // FUTURES_EXECUTOR_DETAIL_IS_EXECUTOR_HPP
