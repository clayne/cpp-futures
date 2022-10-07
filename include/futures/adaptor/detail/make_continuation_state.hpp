//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_ADAPTOR_DETAIL_MAKE_CONTINUATION_STATE_HPP
#define FUTURES_ADAPTOR_DETAIL_MAKE_CONTINUATION_STATE_HPP

#include <futures/detail/operation_state.hpp>
#include <futures/detail/shared_state.hpp>

namespace futures::detail {
    template <
        class value_type,
        class future_options,
        class Executor,
        class Function>
    shared_state<value_type, future_options>
    make_continuation_shared_state(Executor const &ex, Function &&f) {
        using shared_state_t = operation_state<value_type, future_options>;
        if constexpr (!future_options::is_always_deferred) {
            (void) f;
            return std::make_shared<shared_state_t>(ex);
        } else {
            return std::make_shared<
                shared_state_t>(ex, std::forward<Function>(f));
        }
    }
} // namespace futures::detail

#endif // FUTURES_ADAPTOR_DETAIL_MAKE_CONTINUATION_STATE_HPP
