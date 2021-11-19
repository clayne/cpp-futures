//
// Created by Alan Freitas on 8/20/21.
//

#ifndef FUTURES_ALGORITHM_TRAITS_H
#define FUTURES_ALGORITHM_TRAITS_H

/// \file Identify traits for algorithms, like we do for other types
///
/// The traits help us generate auxiliary algorithm overloads
/// This is somewhat similar to the pattern of traits and algorithms for ranges and views
/// It allows us to get algorithm overloads for free, including default inference of the best execution policies
///
/// \see https://en.cppreference.com/w/cpp/ranges/transform_view
/// \see https://en.cppreference.com/w/cpp/ranges/view
///

#include <execution>

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#include <range/v3/range/concepts.hpp>

#include "../detail/default_executor.h"
#include "../detail/inline_executor.h"
#include "partitioner.h"

namespace futures {
    /** \addtogroup algorithms Algorithms
     *  @{
     */

    /** \addtogroup algorithm-traits Algorithm Traits
     *  @{
     */


    /// @name Execution policies

    /// Class representing a type for a sequenced_policy tag
    class sequenced_policy {};

    /// Class representing a type for a parallel_policy tag
    class parallel_policy {};

    /// Class representing a type for a parallel_unsequenced_policy tag
    class parallel_unsequenced_policy {};

    /// Class representing a type for an unsequenced_policy tag
    class unsequenced_policy {};

    /// @name Instances of the execution policy types

    /// \brief Tag used in algorithms for a sequenced_policy
    inline constexpr sequenced_policy seq{};

    /// \brief Tag used in algorithms for a parallel_policy
    inline constexpr parallel_policy par{};

    /// \brief Tag used in algorithms for a parallel_unsequenced_policy
    inline constexpr parallel_unsequenced_policy par_unseq{};

    /// \brief Tag used in algorithms for an unsequenced_policy
    inline constexpr unsequenced_policy unseq{};

    /// \brief Checks whether T is a standard or implementation-defined execution policy type.
    template <class T>
    struct is_execution_policy
        : std::disjunction<std::is_same<T, sequenced_policy>, std::is_same<T, parallel_policy>,
                           std::is_same<T, parallel_unsequenced_policy>, std::is_same<T, unsequenced_policy>> {};

    /// \brief Checks whether T is a standard or implementation-defined execution policy type.
    template <class T> inline constexpr bool is_execution_policy_v = is_execution_policy<T>::value;

    /// \brief Make an executor appropriate to a given policy and a pair of iterators
    /// This depends, of course, of the default executors we have available and
    template <class E, class I, class S,
              std::enable_if_t<!is_executor_v<E> && is_execution_policy_v<E> && ranges::input_iterator<I> &&
                                   ranges::sentinel_for<S, I>,
                               int> = 0>
    constexpr decltype(auto) make_policy_executor() {
        if constexpr (not std::is_same_v<E, sequenced_policy>) {
            return make_default_executor();
        } else {
            return make_inline_executor();
        }
    }

    namespace detail {

        /// \brief CRTP class with the overloads for classes that look for elements in a sequence with an unary function
        /// This includes algorithms such as for_each, any_of, all_of, ...
        template <class Derived> class unary_invoke_algorithm_fn {
          public:
            template <class E, class P, class I, class S, class Fun,
                      std::enable_if_t<is_executor_v<E> && is_partitioner_v<P, I, S> && ranges::input_iterator<I> &&
                                           ranges::sentinel_for<S, I> && ranges::indirectly_unary_invocable<Fun, I> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, P p, I first, S last, Fun f) const {
                return Derived().main(ex, std::forward<P>(p), first, last, f);
            }

            /// \overload execution policy instead of executor
            /// we can't however, count on std::is_execution_policy being defined
            template <
                class E, class P, class I, class S, class Fun,
                std::enable_if_t<not is_executor_v<E> && is_execution_policy_v<E> && is_partitioner_v<P, I, S> &&
                                     ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                     ranges::indirectly_unary_invocable<Fun, I> && std::is_copy_constructible_v<Fun>,
                                 int> = 0>
            decltype(auto) operator()(const E &, P p, I first, S last, Fun f) const {
                return Derived().operator()(make_policy_executor<E, I, S>(), std::forward<P>(p), first, last, f);
            }

            /// \overload Ranges
            template <class E, class P, class R, class Fun,
                      std::enable_if_t<(is_executor_v<E> || is_execution_policy_v<E>)&&is_range_partitioner_v<P, R> &&
                                           ranges::input_range<R> &&
                                           ranges::indirectly_unary_invocable<Fun, ranges::iterator_t<R>> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, P p, R &&r, Fun f) const {
                return Derived().operator()(ex, std::forward<P>(p), std::begin(r), std::end(r), std::move(f));
            }

            /// \overload Iterators / default parallel executor
            template <
                class P, class I, class S, class Fun,
                std::enable_if_t<is_partitioner_v<P, I, S> && ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                     ranges::indirectly_unary_invocable<Fun, I> && std::is_copy_constructible_v<Fun>,
                                 int> = 0>
            decltype(auto) operator()(P p, I first, S last, Fun f) const {
                return Derived().operator()(make_default_executor(), std::forward<P>(p), first, last, std::move(f));
            }

            /// \overload Ranges / default parallel executor
            template <class P, class R, class Fun,
                      std::enable_if_t<is_range_partitioner_v<P, R> && ranges::input_range<R> &&
                                           ranges::indirectly_unary_invocable<Fun, ranges::iterator_t<R>> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(P p, R &&r, Fun f) const {
                return Derived().operator()(make_default_executor(), std::forward<P>(p), std::begin(r), std::end(r),
                                           std::move(f));
            }

            /// \overload Iterators / default partitioner
            template <class E, class I, class S, class Fun,
                      std::enable_if_t<(is_executor_v<E> || is_execution_policy_v<E>)&&ranges::input_iterator<I> &&
                                           ranges::sentinel_for<S, I> && ranges::indirectly_unary_invocable<Fun, I> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, I first, S last, Fun f) const {
                return Derived().operator()(ex, make_default_partitioner(first, last), first, last, std::move(f));
            }

            /// \overload Ranges / default partitioner
            template <class E, class R, class Fun,
                      std::enable_if_t<(is_executor_v<E> || is_execution_policy_v<E>)&&ranges::input_range<R> &&
                                           ranges::indirectly_unary_invocable<Fun, ranges::iterator_t<R>> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, R &&r, Fun f) const {
                return Derived().operator()(ex, make_default_partitioner(std::forward<R>(r)), std::begin(r), std::end(r),
                                           std::move(f));
            }

            /// \overload Iterators / default executor / default partitioner
            template <
                class I, class S, class Fun,
                std::enable_if_t<ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                     ranges::indirectly_unary_invocable<Fun, I> && std::is_copy_constructible_v<Fun>,
                                 int> = 0>
            decltype(auto) operator()(I first, S last, Fun f) const {
                return Derived().operator()(make_default_executor(), make_default_partitioner(first, last), first, last,
                                           std::move(f));
            }

            /// \overload Ranges / default executor / default partitioner
            template <class R, class Fun,
                      std::enable_if_t<ranges::input_range<R> &&
                                           ranges::indirectly_unary_invocable<Fun, ranges::iterator_t<R>> &&
                                           std::is_copy_constructible_v<Fun>,
                                       int> = 0>
            decltype(auto) operator()(R &&r, Fun f) const {
                return Derived().operator()(make_default_executor(), make_default_partitioner(r), std::begin(r),
                                           std::end(r), std::move(f));
            }
        };

        /// \brief CRTP class with the overloads for classes that look for elements in a sequence with an unary function
        /// This includes algorithms such as for_each, any_of, all_of, ...
        template <class Derived> class value_cmp_algorithm_fn {
          public:
            template <class E, class P, class I, class S, class T,
                      std::enable_if_t<is_executor_v<E> && is_partitioner_v<P, I, S> && ranges::input_iterator<I> &&
                                           ranges::sentinel_for<S, I> &&
                                           ranges::indirectly_binary_invocable_<ranges::equal_to, T *, I>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, P p, I first, S last, T f) const {
                return Derived().main(ex, std::forward<P>(p), first, last, f);
            }

            /// \overload execution policy instead of executor
            /// we can't however, count on std::is_execution_policy being defined
            template <class E, class P, class I, class S, class T,
                      std::enable_if_t<not is_executor_v<E> && is_execution_policy_v<E> && is_partitioner_v<P, I, S> &&
                                           ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                           ranges::indirectly_binary_invocable_<ranges::equal_to, T *, I>,
                                       int> = 0>
            decltype(auto) operator()(const E &, P p, I first, S last, T f) const {
                return Derived().operator()(make_policy_executor<E, I, S>(), std::forward<P>(p), first, last, f);
            }

            /// \overload Ranges
            template <class E, class P, class R, class T,
                      std::enable_if_t<
                          (is_executor_v<E> || is_execution_policy_v<E>)&&is_range_partitioner_v<P, R> &&
                              ranges::input_range<R> &&
                              ranges::indirectly_binary_invocable_<ranges::equal_to, T *, ranges::iterator_t<R>> &&
                              std::is_copy_constructible_v<T>,
                          int> = 0>
            decltype(auto) operator()(const E &ex, P p, R &&r, T f) const {
                return Derived().operator()(ex, std::forward<P>(p), std::begin(r), std::end(r), std::move(f));
            }

            /// \overload Iterators / default parallel executor
            template <
                class P, class I, class S, class T,
                std::enable_if_t<is_partitioner_v<P, I, S> && ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                     ranges::indirectly_binary_invocable_<ranges::equal_to, T *, I> &&
                                     std::is_copy_constructible_v<T>,
                                 int> = 0>
            decltype(auto) operator()(P p, I first, S last, T f) const {
                return Derived().operator()(make_default_executor(), std::forward<P>(p), first, last, std::move(f));
            }

            /// \overload Ranges / default parallel executor
            template <class P, class R, class T,
                      std::enable_if_t<
                          is_range_partitioner_v<P, R> && ranges::input_range<R> &&
                              ranges::indirectly_binary_invocable_<ranges::equal_to, T *, ranges::iterator_t<R>> &&
                              std::is_copy_constructible_v<T>,
                          int> = 0>
            decltype(auto) operator()(P p, R &&r, T f) const {
                return Derived().operator()(make_default_executor(), std::forward<P>(p), std::begin(r), std::end(r),
                                           std::move(f));
            }

            /// \overload Iterators / default partitioner
            template <class E, class I, class S, class T,
                      std::enable_if_t<(is_executor_v<E> || is_execution_policy_v<E>)&&ranges::input_iterator<I> &&
                                           ranges::sentinel_for<S, I> &&
                                           ranges::indirectly_binary_invocable_<ranges::equal_to, T *, I>,
                                       int> = 0>
            decltype(auto) operator()(const E &ex, I first, S last, T f) const {
                return Derived().operator()(ex, make_default_partitioner(first, last), first, last, std::move(f));
            }

            /// \overload Ranges / default partitioner
            template <class E, class R, class T,
                      std::enable_if_t<
                          (is_executor_v<E> || is_execution_policy_v<E>)&&ranges::input_range<R> &&
                              ranges::indirectly_binary_invocable_<ranges::equal_to, T *, ranges::iterator_t<R>> &&
                              std::is_copy_constructible_v<T>,
                          int> = 0>
            decltype(auto) operator()(const E &ex, R &&r, T f) const {
                return Derived().operator()(ex, make_default_partitioner(std::forward<R>(r)), std::begin(r), std::end(r),
                                           std::move(f));
            }

            /// \overload Iterators / default executor / default partitioner
            template <class I, class S, class T,
                      std::enable_if_t<ranges::input_iterator<I> && ranges::sentinel_for<S, I> &&
                                           ranges::indirectly_binary_invocable_<ranges::equal_to, T *, I>,
                                       int> = 0>
            decltype(auto) operator()(I first, S last, T f) const {
                return Derived().operator()(make_default_executor(), make_default_partitioner(first, last), first, last,
                                           std::move(f));
            }

            /// \overload Ranges / default executor / default partitioner
            template <class R, class T,
                      std::enable_if_t<
                          ranges::input_range<R> &&
                              ranges::indirectly_binary_invocable_<ranges::equal_to, T *, ranges::iterator_t<R>> &&
                              std::is_copy_constructible_v<T>,
                          int> = 0>
            decltype(auto) operator()(R &&r, T f) const {
                return Derived().operator()(make_default_executor(), make_default_partitioner(r), std::begin(r),
                                           std::end(r), std::move(f));
            }
        };
    } // namespace detail
    /** @}*/ // \addtogroup algorithm-traits Algorithm Traits
    /** @}*/ // \addtogroup algorithms Algorithms
} // namespace futures

#endif // FUTURES_ALGORITHM_TRAITS_H
