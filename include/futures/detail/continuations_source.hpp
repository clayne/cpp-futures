//
// Copyright (c) 2021 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_DETAIL_CONTINUATIONS_SOURCE_HPP
#define FUTURES_DETAIL_CONTINUATIONS_SOURCE_HPP

#include <futures/config.hpp>
#include <futures/executor/execute.hpp>
#include <futures/detail/container/atomic_queue.hpp>
#include <futures/detail/container/small_vector.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace futures {
    namespace detail {
        /** @addtogroup futures Futures
         *  @{
         */

        // The continuation state as a small thread safe container that
        // holds continuation functions for a future
        //
        // The whole logic here is very similar to that of stop_tokens. There
        // is a source, a state, and a token.
        //
        // This is very limited as a container because there are not many
        // operations we need to do with the continuation state. We need to be
        // able to attach continuations (then), and run all continuations with
        // a single shared lock.
        //
        // Like the stop_state, a continuation state might be shared between
        // shared futures. Once one of the futures has run the continuations,
        // the state is considered done.
        //
        // The continuation state needs to be atomic because it's also a shared
        // state. Especially when the future is shared, many threads might be
        // trying to attach new continuations to this future type, and the main
        // future callback needs to wait for it.
        template <bool is_always_deferred>
        class continuations_state {
        public:
            // @name Public Types
            // @{

            // Type of a continuation callback
            // This is a callback function that posts the next task to an
            // executor. We cannot ensure the tasks go to the same executor.
            // This needs to be type erased because there are many types of
            // callables that might become a continuation here.
            using continuation_type = std::function<void()>;

            // The continuation vector
            // We use a small vector because of the common case when there few
            // continuations per task
            using continuation_vector = std::conditional_t<
                !is_always_deferred,
                detail::atomic_queue<continuation_type>,
                detail::small_vector<continuation_type>>;

            // @}

            // @name Constructors
            // @{

            // Default constructor
            continuations_state() = default;

            // Copy constructor
            continuations_state(continuations_state const &) = delete;

            // Destructor - Run continuations if they have not run yet
            ~continuations_state() {
                request_run();
            }

            // Copy assignment
            continuations_state &
            operator=(continuations_state const &)
                = delete;

            // @}

            // @name Modifying
            // @{

            // Check if some source asked already asked for the
            // continuations to run
            bool
            is_run_requested() const {
                return is_run_requested(mp_bool<is_always_deferred>{});
            }
        private:
            bool
            is_run_requested(std::true_type /* is_always_deferred */) const {
                return run_requested_;
            }

            bool
            is_run_requested(std::false_type /* is_always_deferred */) const {
                return run_requested_.load(std::memory_order_acquire);
            }

        public:
            // Check if some source asked already asked for the
            // continuations to run
            bool
            is_run_possible() const {
                return !is_run_requested();
            }

            // Emplace a new continuation
            // Use executor ex if more continuations are not possible
            template <class Executor, class Fn>
            bool
            push(Executor const &ex, Fn &&fn) {
                // Although this is a write operation, this is a shared lock
                // because many threads are allowed to emplace continuations
                // at the same time in the atomic queue.
                std::unique_lock<std::mutex> lock(continuations_mutex_);
                if (!is_run_requested()) {
                    push_continuation(continuations_, std::forward<Fn>(fn));
                    return true;
                } else {
                    lock.unlock();
                    // When the shared state currently associated with *this is
                    // ready, the continuation is called on an unspecified
                    // thread of execution
                    execute(ex, std::forward<Fn>(fn));
                    return false;
                }
            }

        private:
            template <class Fn>
            static void
            push_continuation(
                detail::atomic_queue<continuation_type> &v,
                Fn &&fn) {
                v.push(std::forward<Fn>(fn));
            }

            template <class Fn>
            static void
            push_continuation(
                detail::small_vector<continuation_type> &v,
                Fn &&fn) {
                v.emplace_back(std::forward<Fn>(fn));
            }

        public:
            // Run all continuations
            bool
            request_run() {
                return request_run(mp_bool<is_always_deferred>{});
            }
            // @}

        private:
            bool
            request_run(std::true_type /* is_always_deferred */) {
                if (!run_requested_) {
                    run_requested_ = true;
                    for (auto &c: continuations_) {
                        c();
                    }
                    continuations_.clear();
                    return true;
                }
                return false;
            }

            bool
            request_run(std::false_type /* is_always_deferred */) {
                bool prev_request = false;
                if (run_requested_.compare_exchange_strong(prev_request, true))
                {
                    // Do not lock yet, pop and execute what we can
                    while (!continuations_.empty()) {
                        continuations_.pop()();
                    }
                    // Maybe some other thread was pushing a task while we
                    // were popping. Lock the continuations now to make sure
                    // we wait for that to happen and pop whatever is left.
                    std::unique_lock<std::mutex> lock(continuations_mutex_);
                    while (!continuations_.empty()) {
                        continuations_.pop()();
                    }
                    return true;
                }
                return false;
            }

        private:
            // The actual pointers to the continuation functions
            // This is encapsulated so we can't break anything
            continuation_vector continuations_;

            // A mutex for the continuations.
            //
            // Although the continuations are in an atomic queue and multiple
            // threads can push continuations at the same time, we cannot
            // allow more continuations to be emplaced once we start
            // dequeueing them.
            mutable std::mutex continuations_mutex_;

            // Run has already been requested
            std::conditional_t<!is_always_deferred, std::atomic<bool>, bool>
                run_requested_{ false };
        };

        // Unit type intended for use as a placeholder in continuations_source
        // non-default constructor
        struct nocontinuationsstate_t {
            explicit nocontinuationsstate_t() = default;
        };

        // This is a constant object instance of stdnocontinuationsstate_t for
        // use in constructing an empty continuations_source, as a placeholder
        // value in the non-default constructor
        FUTURES_INLINE_VAR constexpr nocontinuationsstate_t
            nocontinuationsstate{};

        // Token the future object uses to emplace continuations
        template <bool is_always_deferred>
        class continuations_token {
        public:
            // Constructs an empty continuations_token with no associated
            // continuations-state
            continuations_token() noexcept : state_(nullptr) {}

            // Constructs a continuations_token whose associated
            // continuations-state is the same as that of other
            continuations_token(continuations_token const &other) noexcept
                = default;

            // Constructs a continuations_token whose associated
            // continuations-state is the same as that of other; other is left
            // empty
            continuations_token(continuations_token &&other) noexcept = default;

            // Copy-assigns the associated continuations-state of other to
            // that of *this
            continuations_token &
            operator=(continuations_token const &other) noexcept
                = default;

            // Move-assigns the associated continuations-state of other to
            // that of *this
            continuations_token &
            operator=(continuations_token &&other) noexcept
                = default;

            // Exchanges the associated continuations-state of *this and
            // other.
            void
            swap(continuations_token &other) noexcept {
                std::swap(state_, other.state_);
            }

            // Checks if the continuations_token object has associated
            // continuations-state and that state has received a run request
            FUTURES_NODISCARD bool
            run_requested() const noexcept {
                return (state_ != nullptr) && state_->is_run_requested();
            }

            // Checks if the continuations_token object has associated
            // continuations-state, and that state either has already had a run
            // requested or it has associated continuations_source object(s)
            FUTURES_NODISCARD bool
            run_possible() const noexcept {
                return (state_ != nullptr) && (!state_->is_run_requested());
            }

            // compares two std::run_token objects
            FUTURES_NODISCARD friend bool
            operator==(
                continuations_token const &lhs,
                continuations_token const &rhs) noexcept {
                return lhs.state_ == rhs.state_;
            }

            FUTURES_NODISCARD friend bool
            operator!=(
                continuations_token const &lhs,
                continuations_token const &rhs) noexcept {
                return lhs.state_ != rhs.state_;
            }

        private:
            template <bool is_always_deferred_source>
            friend class continuations_source;

            // Create token from state
            explicit continuations_token(
                std::shared_ptr<continuations_state<is_always_deferred>>
                    state) noexcept
                : state_(std::move(state)) {}

            // The state
            std::shared_ptr<continuations_state<is_always_deferred>> state_;
        };

        // The continuations_source class provides the means to issue a
        // request to run the future continuations
        template <bool is_always_deferred>
        class continuations_source {
        public:
            // Constructs a continuations_source with new continuations-state
            continuations_source()
                : state_(std::make_shared<
                         continuations_state<is_always_deferred>>()){};

            // Constructs an empty continuations_source with no associated
            // continuations-state.
            explicit continuations_source(nocontinuationsstate_t) noexcept
                : state_{ nullptr } {}

            // Copy constructor.
            // Constructs a continuations_source whose associated
            // continuations-state is the same as that of other.
            continuations_source(continuations_source const &other) noexcept
                = default;

            // Move constructor.
            // Constructs a continuations_source whose associated
            // continuations-state is the same as that of other; other is left
            // empty.
            continuations_source(continuations_source &&other) noexcept
                = default;

            // Copy-assigns the continuations-state of other to that of *this
            continuations_source &
            operator=(continuations_source const &other) noexcept
                = default;

            // Move-assigns the continuations-state of other to that of *this
            continuations_source &
            operator=(continuations_source &&other) noexcept
                = default;

            // Run all continuations
            // The return reference is safe because the continuation vector has
            // stability
            bool
            request_run() {
                if (state_ != nullptr) {
                    return state_->request_run();
                }
                return false;
            }

            // Run all continuations
            // The return reference is safe because the continuation vector has
            // stability
            template <class Executor, class Fn>
            bool
            push(Executor const &ex, Fn &&fn) {
                if (state_ != nullptr) {
                    return state_->push(ex, std::forward<Fn>(fn));
                }
                return false;
            }

            // Exchanges the continuations-state of *this and other.
            void
            swap(continuations_source &other) noexcept {
                std::swap(state_, other.state_);
            }

            // Get a token to this object
            //
            // Returns a continuations_token object associated with the
            // continuations_source's continuations-state, if the
            // continuations_source has continuations-state; otherwise returns
            // a default-constructed (empty) continuations_token.
            FUTURES_NODISCARD continuations_token<is_always_deferred>
            get_token() const noexcept {
                return { state_ };
            }

            // Checks if the continuations_source object has a
            // continuations-state and that state has received a run request.
            FUTURES_NODISCARD bool
            run_requested() const noexcept {
                return state_ != nullptr && state_->is_run_requested();
            }

            // Checks if the continuations_source object has a
            // continuations-state.
            FUTURES_NODISCARD bool
            run_possible() const noexcept {
                return state_ != nullptr;
            }

            // Compares two continuations_source values
            FUTURES_NODISCARD friend bool
            operator==(
                continuations_source const &a,
                continuations_source const &b) noexcept {
                return a.state_ == b.state_;
            }

            // Compares two continuations_source values
            FUTURES_NODISCARD friend bool
            operator!=(
                continuations_source const &a,
                continuations_source const &b) noexcept {
                return a.state_ != b.state_;
            }

        private:
            std::shared_ptr<continuations_state<is_always_deferred>> state_;
        };

        /** @} */
    } // namespace detail
} // namespace futures

#endif // FUTURES_DETAIL_CONTINUATIONS_SOURCE_HPP
