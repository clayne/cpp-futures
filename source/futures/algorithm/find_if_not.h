//
// Created by Alan Freitas on 8/16/21.
//

#ifndef FUTURES_FIND_IF_NOT_H
#define FUTURES_FIND_IF_NOT_H

#include <execution>
#include <variant>

#include <range/v3/all.hpp>

#include "../futures.h"
#include "algorithm_traits.h"
#include "partitioner.h"

namespace futures {
    /** \addtogroup algorithms Algorithms
     *  @{
     */

    /// Class representing the overloads for the @ref find_if_not function
    class find_if_not_fn : public detail::unary_invoke_algorithm_fn<find_if_not_fn> {
      public:
        /// \brief Complete overload of the find_if_not algorithm
        /// \tparam E Executor type
        /// \tparam P Partitioner type
        /// \tparam I Iterator type
        /// \tparam S Sentinel iterator type
        /// \tparam Fun Function type
        /// \param ex Executor
        /// \param p Partitioner
        /// \param first Iterator to first element in the range
        /// \param last Iterator to (last + 1)-th element in the range
        /// \param f Function
        /// \brief function template \c find_if_not
        template <class E, class P, class I, class S, class Fun,
                  std::enable_if_t<is_executor_v<E> && is_partitioner_v<P, I, S> && ranges::input_iterator<I> &&
                                       ranges::sentinel_for<S, I> && ranges::indirectly_unary_invocable<Fun, I> &&
                                       std::is_copy_constructible_v<Fun>,
                                   int> = 0>
        I main(const E &ex, P p, I first, S last, Fun f) const {
            auto middle = p(first, last);
            if (middle == last || std::is_same_v<E, inline_executor> || ranges::forward_iterator<I>) {
                return std::find_if_not(first, last, f);
            }

            // Run find_if_not on rhs: [middle, last]
            auto [rhs, rhs_started, rhs_cancel] = try_async(ex, [=]() { return operator()(ex, p, middle, last, f); });

            // Run find_if_not on lhs: [first, middle]
            I lhs = operator()(ex, p, first, middle, f);

            // Wait for rhs
            if (is_ready(rhs_started)) {
                rhs.wait();
                if (lhs != middle) {
                    return lhs;
                } else {
                    return rhs.get();
                }
            } else {
                rhs_cancel.request_stop();
                rhs.detach();
                if (lhs != middle) {
                    return lhs;
                } else {
                    return operator()(make_inline_executor(), p, middle, last, f);
                }
            }
        }
    };

    /// \brief Finds the first element not satisfying specific criteria
    inline constexpr find_if_not_fn find_if_not;

    /** @}*/ // \addtogroup algorithms Algorithms
} // namespace futures

#endif // FUTURES_FIND_IF_NOT_H
