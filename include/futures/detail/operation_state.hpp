//
// Copyright (c) 2021 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_DETAIL_OPERATION_STATE_HPP
#define FUTURES_DETAIL_OPERATION_STATE_HPP

#include <futures/config.hpp>
#include <futures/error.hpp>
#include <futures/future_options.hpp>
#include <futures/future_status.hpp>
#include <futures/throw.hpp>
#include <futures/detail/container/small_vector.hpp>
#include <futures/detail/continuations_source.hpp>
#include <futures/detail/operation_state_storage.hpp>
#include <futures/detail/thread/relocker.hpp>
#include <futures/detail/utility/compressed_tuple.hpp>
#include <futures/detail/utility/maybe_atomic.hpp>
#include <futures/detail/utility/regular_void.hpp>
#include <futures/adaptor/detail/continue.hpp>
#include <futures/adaptor/detail/future_continue_task.hpp>
#include <futures/detail/deps/boost/core/empty_value.hpp>
#include <futures/detail/deps/boost/core/ignore_unused.hpp>
#include <futures/detail/deps/boost/mp11/algorithm.hpp>
#include <futures/detail/deps/boost/mp11/list.hpp>
#include <atomic>
#include <condition_variable>

namespace futures {
    namespace detail {
        /** @addtogroup futures Futures
         *  @{
         */

        /// Operation state common synchronization primitives
        /**
         * Member functions and objects common to all operation state object
         * types.
         *
         * Operation states for asynchronous operations contain an element of a
         * given type or an exception.
         *
         * All objects such as futures and promises have operation states and
         * inherit from this class to synchronize their access to their common
         * operation state.
         *
         * When we know the operation state is always deferred, we can use some
         * optimizations related to their synchronization. In other words, we
         * can avoid atomic operations necessary to determine the state of the
         * task and its continuations.
         *
         * @tparam is_always_deferred Whether the state is always deferred
         */
        class operation_state_base {
        private:
            /**
             * @name Private types
             * @{
             */

            /// Type used for a list of external waiters
            /**
             * A list of waiters is a list of references to external condition
             * variables we should notify when this operation state is ready.
             *
             * This is an extension useful to implement wait_for_any, whose
             * alternative is to add a number of continuations setting a
             * common flag indicating a task has finished.
             *
             */
            using waiter_list = small_vector<std::condition_variable_any *>;

            /**
             * @}
             */

        public:
            /**
             * @name Public types
             * @{
             */

            /// A handle to notify an external context about this state being
            /// ready
            using notify_when_ready_handle = waiter_list::iterator;

            /**
             * @}
             */

            /**
             * @name Constructors
             * @{
             */

            /// Virtual operation state data destructor
            virtual ~operation_state_base() {
                // Wait for any operations to complete
                auto lk = make_wait_lock();
            };

            /// Constructor
            /**
             * Deleted to ensure we always explicitly define the initial
             * status of the operation state.
             */
            operation_state_base() = delete;

            /// Constructor
            /**
             * Defaults to the initial status for the operation state.
             *
             * The initial status is already launched for eager futures.
             */
            explicit operation_state_base(bool is_deferred)
                : status_(is_deferred ? status::deferred : status::launched) {}

            /// Deleted copy constructor
            /**
             *  We should not copy construct the operation state data because
             * its members, which include synchronization primitives, should be
             * shared in eager futures.
             *
             *  In deferred futures, copying would still represent a logic error
             *  at a higher level. A shared deferred future will still use a
             *  shared operation state, while a unique deferred future is not
             *  allowed to be copied.
             */
            operation_state_base(operation_state_base const &) = delete;

            /// Deleted copy assignment
            /**
             * We cannot copy assign the operation state data because this base
             * class holds synchronization primitives.
             */
            operation_state_base &
            operator=(operation_state_base const &)
                = delete;

            /// Move constructor
            /**
             * Moving the operation state is only valid before the task is
             * running, as it might happen with deferred futures. This allows us
             * to change the address of the deferred future without having to
             * share it.
             *
             * At this point, we can ignore/recreate the unused synchronization
             * primitives, such as condition variables and mutexes, and let
             * other objects steal the contents of the base class while
             * recreating the condition variables.
             *
             * This is only supposed to be used by deferred futures being
             * shared, which means
             *
             * 1) the task hasn't been launched yet,
             * 2) their `operation_state_base` is inline in the future and would
             *    become shared otherwise.
             *
             * Thus, this allows us to (i) move deferred futures with inline
             * operation states, or (ii) to construct a shared_ptr from an
             * existing operation state, in case it needs to be shared.
             *
             */
            operation_state_base(operation_state_base &&other) noexcept
                : status_{ other.status_ }
                , except_{ std::move(other.except_) }
                , external_waiters_(std::move(other.external_waiters_)) {
                std::unique_lock<std::mutex> lk(other.waiters_mutex_);
                assert(!is_running());
                other.status_ = status::ready;
            };

            /// Move assignment
            /**
             * Moving the operation state is only valid before the task is
             * running, as it might happen with deferred futures. This allows us
             * to change the address of the deferred future without having to
             * share it.
             *
             * At this point, we can ignore/recreate the unused synchronization
             * primitives, such as condition variables and mutexes, and let
             * other objects steal the contents of the base class while
             * recreating the condition variables.
             *
             * This is only supposed to be used by deferred futures being
             * shared, which means
             *
             * 1) the task hasn't been launched yet,
             * 2) their `operation_state_base` is inline in the future and would
             *    become shared otherwise.
             *
             * Thus, this allows us to (i) move deferred futures with inline
             * operation states, or (ii) to construct a shared_ptr from an
             * existing operation state, in case it needs to be shared.
             *
             */
            operation_state_base &
            operator=(operation_state_base &&other) noexcept {
                status_ = std::move(other.status_);
                except_ = std::move(other.except_);
                external_waiters_ = std::move(other.external_waiters_);
                std::unique_lock<std::mutex> lk(other.waiters_mutex_);
                assert(!is_running());
                other.status_ = status::ready;
                return *this;
            };

            /**
             * @}
             */

            /**
             * @name Observers
             *
             * Determine the current state of the shared state.
             * All of these operations are individually unsafe.
             *
             * @{
             */

            /// Check if operation state is currently deferred
            bool
            is_deferred() const {
                return status_ == status::deferred;
            }

            /// Check if operation state has been launched
            bool
            is_launched() const {
                return status_ == status::launched;
            }

            /// Check if operation state is waiting
            bool
            is_waiting() const {
                return status_ == status::waiting;
            }

            /// Check if operation state is ready
            bool
            is_ready() const {
                return status_ == status::ready;
            }

            /// Check if associated task is running
            bool
            is_running() const {
                return status_ == status::launched
                       || status_ == status::waiting;
            }

            /// Check if state is ready with no exception
            bool
            succeeded() const {
                return is_ready() && except_ == nullptr;
            }

            /// Check if state is ready without locking
            bool
            failed() const {
                return is_ready() && except_ != nullptr;
            }

            /**
             * @}
             */

            /**
             * @name Accessors
             *
             * The accessor functions are used to mark a change to the state of
             * the operation. They will trigger and use whatever synchronization
             * primitives are necessary to avoid data races between promises
             * and futures.
             *
             * This base object only handles flags, while the base classes are
             * concerned with the state storage and extensions, if any.
             *
             * @{
             */

            /// Mark operation state as ready
            /**
             *  This operation marks the status flag as ready and warns any
             * future waiting for them.
             *
             *  We only block the thread to notify futures if there are any
             *  threads really waiting for this operation state.
             *
             *  This overload is meant to be used by derived classes that will
             *  also set the state of their storage.
             */
            void
            mark_ready() noexcept {
                auto lk = make_wait_lock();
                mark_ready_unsafe();
            }

            /// Set operation state to an exception
            /**
             *  This sets the exception value and marks the operation state as
             *  ready.
             *
             *  If we try to set an exception on a operation state that's ready,
             * we throw an exception.
             *
             *  This overload is meant to be used by derived classes.
             */
            void
            mark_exception(std::exception_ptr except) {
                auto lk = make_wait_lock();
                mark_exception_unsafe(std::move(except));
            }

            /// Get the operation state when it's as an exception
            std::exception_ptr
            get_exception_ptr() const {
                auto lk = make_wait_lock();
                if (!is_ready()) {
                    throw_exception(promise_uninitialized{});
                }
                return except_;
            }

            /// Rethrow the exception we have stored
            void
            throw_internal_exception() const {
                std::rethrow_exception(get_exception_ptr());
            }

            /// Indicate to the operation state its owner has been destroyed
            /**
             *  Promise types, such as promise and packaged task, call this
             * function to allows us to set an error if the promise has been
             * destroyed too early.
             *
             *  If the owner has been destroyed before the operation state is
             * ready, this means a promise has been broken and the operation
             * state should store an exception.
             */
            void
            signal_promise_destroyed() {
                if (!is_ready()) {
                    mark_exception(std::make_exception_ptr(broken_promise()));
                }
            }


            /**
             * @}
             */

            /**
             * @name Waiting
             * @{
             */

            /// Wait for operation state to become ready
            /**
             *  This function uses the condition variable waiters to wait for
             *  this operation state to be marked as ready.
             */
            void
            wait() {
                wait_impl<false>(*this);
            }

            /// Wait for operation state to become ready
            void
            wait() const {
                wait_impl<true>(*this);
            }

            /// Wait for the operation state to become ready
            /**
             *  This function uses the condition variable waiters to wait for
             *  this operation state to be marked as ready for a specified
             *  duration.
             *
             *  @tparam Rep An arithmetic type representing the number of ticks
             *  @tparam Period A std::ratio representing the tick period
             *
             *  @param timeout_duration maximum duration to block for
             *
             *  @return The state of the operation value
             */
            template <typename Rep, typename Period>
            future_status
            wait_for(
                std::chrono::duration<Rep, Period> const &timeout_duration) {
                auto timeout_time = std::chrono::system_clock::now()
                                    + timeout_duration;
                return wait_impl<false>(*this, &timeout_time);
            }

            /// Wait for the operation state to become ready
            template <typename Rep, typename Period>
            future_status
            wait_for(std::chrono::duration<Rep, Period> const &timeout_duration)
                const {
                auto timeout_time = std::chrono::system_clock::now()
                                    + timeout_duration;
                return wait_impl<true>(*this, &timeout_time);
            }

            /// Wait for the operation state to become ready
            /**
             *  This function uses the condition variable waiters
             *  to wait for this operation state to be marked as ready until a
             *  specified time point.
             *
             *  @tparam Clock The clock type
             *  @tparam Duration The duration type
             *
             *  @param timeout_time maximum time point to block until
             *
             *  @return The state of the operation value
             */
            template <typename Clock, typename Duration>
            future_status
            wait_until(
                std::chrono::time_point<Clock, Duration> const &timeout_time) {
                return wait_impl<false>(*this, &timeout_time);
            }

            /// Wait for the operation state to become ready
            template <typename Clock, typename Duration>
            future_status
            wait_until(std::chrono::time_point<Clock, Duration> const
                           &timeout_time) const {
                return wait_impl<true>(*this, &timeout_time);
            }

            /**
             * @}
             */

            /**
             * @name Synchronization
             * @{
             */

            /// Include an external condition variable in the list of waiters
            /**
             *  These are external waiters we need to notify when the state is
             *  ready.
             *
             *  @param cv Reference to an external condition variable
             *
             *  @return Handle which can be used to unnotify when ready
             */
            notify_when_ready_handle
            notify_when_ready(std::condition_variable_any &cv) {
                auto lk = make_wait_lock();
                status prev = status_;
                // wait for parent task
                wait_for_parent();
                if (prev == status::deferred) {
                    status_ = status::launched;
                    this->post_deferred();
                }
                if (prev != status::ready) {
                    status_ = status::waiting;
                }
                return external_waiters_.insert(external_waiters_.end(), &cv);
            }

            /// Remove condition variable from list of external waiters
            /**
             *  @param it External condition variable
             */
            void
            unnotify_when_ready(notify_when_ready_handle it) {
                auto lk = make_wait_lock();
                external_waiters_.erase(it);
            }

            /// Post a deferred function
            virtual void
            post_deferred() {
                // do nothing by default / assume tasks are eager
                // override in deferred futures only
            }

            /// Wait for parent operation
            virtual void
            wait_for_parent() {
                // do nothing by default / assume tasks are eager
                // override in deferred futures only
            }

            /// Get a reference to the mutex in the operation state
            std::mutex &
            waiters_mutex() {
                return waiters_mutex_;
            }

            /// Generate unique lock for the operation state
            /**
             *  This lock can be used for any operations on the state that might
             *  need to be protected.
             */
            std::unique_lock<std::mutex>
            make_wait_lock() const {
                return std::unique_lock<std::mutex>{ waiters_mutex_ };
            }

            /**
             * @}
             */

        protected:
            /**
             * @name Private functions
             * @{
             */

            /// Mark operation state as ready without synchronization
            void
            mark_ready_unsafe() noexcept {
                status prev = status_;
                status_ = status::ready;
                if (prev == status::waiting) {
                    waiter_.notify_all();
                    for (auto &&external_waiter: external_waiters_) {
                        external_waiter->notify_all();
                    }
                }
            }

            /// Set operation state to an exception without synchronization
            void
            mark_exception_unsafe(std::exception_ptr except) {
                if (is_ready()) {
                    throw_exception(promise_already_satisfied{});
                }
                except_ = std::move(except);
                mark_ready_unsafe();
            }

            template <bool is_const>
            using maybe_const_this = std::conditional_t<
                is_const,
                const operation_state_base,
                operation_state_base>;

        private:
            static void
            post_deferred_impl(
                operation_state_base const &,
                bool,
                std::unique_lock<std::mutex> &) {}

            static void
            post_deferred_impl(
                operation_state_base &op,
                bool prev_is_deferred,
                std::unique_lock<std::mutex> &lk) {
                op.wait_for_parent();
                if (prev_is_deferred) {
                    op.status_ = status::launched;
                    relocker rlk(lk);
                    op.post_deferred();
                }
            }

            template <
                bool is_const,
                typename Clock = std::chrono::system_clock,
                typename Duration = std::chrono::system_clock::duration>
            static future_status
            wait_impl(
                maybe_const_this<is_const> &op,
                std::chrono::time_point<Clock, Duration> const *timeout_time
                = nullptr) {
                std::unique_lock<std::mutex> lk = op.make_wait_lock();
                FUTURES_IF_CONSTEXPR (is_const) {
                    if (op.is_deferred()) {
                        return future_status::deferred;
                    }
                }
                status prev = op.status_;
                if (prev != status::ready) {
                    // Only the mutable version is allowed to post the deferred
                    // task.
                    post_deferred_impl(op, prev == status::deferred, lk);
                    prev = op.status_;
                    if (prev != status::ready) {
                        op.status_ = status::waiting;
                        if (timeout_time) {
                            if (op.waiter_
                                    .wait_until(lk, *timeout_time, [&op]() {
                                        return op.is_ready();
                                    }))
                            {
                                return future_status::ready;
                            } else {
                                op.status_ = status::launched;
                                return future_status::timeout;
                            }
                        } else {
                            op.waiter_.wait(lk, [&op]() {
                                return op.is_ready();
                            });
                            return future_status::ready;
                        }
                    }
                }
                return future_status::ready;
            }

            /**
             * @}
             */

            /**
             * @name Member values
             * @{
             */


            /// The current status of this operation state
            enum status : uint8_t
            {
                /// Nothing happened yet
                deferred,
                /// The task has been launched
                launched,
                /// Some thread is waiting for the result
                waiting,
                /// The state has been set and we notified everyone
                ready,
            };

            /// Indicates if the operation state is already set
            mutable status status_;

            /// Pointer to an exception, when the operation_state fails
            /**
             *  std::exception_ptr does not need to be atomic because
             *  the status variable is guarding it.
             */
            std::exception_ptr except_{ nullptr };

            /// Condition variable to notify any object waiting for this state
            /**
             *  This is the object we use to be able to block until the
             * operation state is ready. Although the operation state is
             * lock-free, users might still need to wait for results to be
             * ready. One future thread calls `waiter_.wait(...)` to block while
             * the promise thread calls `waiter_.notify_all(...)`. In C++20,
             * atomic variables have `wait()` functions we could use to replace
             * this with `ready_.wait(...)`
             */
            mutable std::condition_variable waiter_{};

            /// List of external condition variables waiting for the state
            /**
             *  While the internal waiter is intended for
             */
            waiter_list external_waiters_;

            /// Mutex for threads that want to wait on the result
            /**
             *  While the basic state operations is lock-free, it also includes
             * a mutex that can be used for communication between futures, such
             * as waiter futures.
             *
             *  This is used when one thread intents to wait for the result
             *  of a future. The mutex is not used for lazy futures or if
             *  the result is ready before the waiting operation.
             *
             *  These functions should be used directly by users very often.
             */
            mutable std::mutex waiters_mutex_{};

            /**
             * @}
             */
        };

        /// Operation state with its concrete storage
        /**
         * This class stores the data for a operation state holding an element
         * of type `R`, which might be a concrete type, a reference, or `void`.
         *
         * For most types, the data is stored as uninitialized storage because
         * this will only need to be initialized when the state becomes ready.
         * This ensures the operation state works for all types and avoids
         * wasting operations on a constructor we might not use.
         *
         * However, initialized storage is used for trivial types because
         * this involves no performance penalty. This also makes states
         * easier to debug.
         *
         * If the operation state returns a reference, the data is stored
         * internally as a pointer.
         *
         * A void operation state needs to synchronize waiting, but it does not
         * need to store anything.
         *
         * The storage also uses empty base optimization, which means it
         * takes no memory if the state is an empty type, such as `void` or
         * std::allocator.
         */
        template <class R, class Options>
        class operation_state : public operation_state_base {
        protected:
            FUTURES_STATIC_ASSERT_MSG(
                !Options::is_shared,
                "The underlying operation state cannot be shared");

            // Get type T if B is true (as a type), F otherwise
            template <class B, class T, class F>
            using constant_conditional_fn = std::conditional_t<B::value, T, F>;

            // Values we should store in the tuple with the members
            using tuple_value_types = mp_transform<
                constant_conditional_fn,
                mp_list<
                    mp_bool<Options::has_executor>,
                    mp_bool<Options::is_continuable>,
                    mp_bool<Options::is_stoppable>,
                    mp_bool<true>>,
                mp_list<
                    typename Options::executor_t,
                    continuations_source<Options::is_always_deferred>,
                    stop_source,
                    operation_state_storage<R>>,
                mp_repeat_c<mp_list<boost::empty_init_t>, 4>>;

            using layout_tuple_type
                = mp_apply<compressed_tuple, tuple_value_types>;

            using stop_token_type = std::conditional_t<
                Options::is_stoppable,
                stop_token,
                boost::empty_init_t>;

            // State member objects represented as a layout with no empty types
            layout_tuple_type layout_;

        public:
            /**
             * @name Public types
             * @{
             */

            using value_type = R;
            using executor_type = typename layout_tuple_type::
                template value_type<0>;
            using continuations_type = typename layout_tuple_type::
                template value_type<1>;
            using stop_source_type = typename layout_tuple_type::
                template value_type<2>;
            using storage_type = typename layout_tuple_type::
                template value_type<3>;

            /**
             * @}
             */

            /**
             * @name Constructors
             * @{
             */

            /// Destructor
            /**
             *  This might destroy the operation state object R if the state is
             *  ready with a value. This logic is encapsulated into the
             *  operation state storage.
             */
            ~operation_state() override {
                destroy_impl(mp_bool<Options::is_stoppable>{});
            };

        private:
            void
            destroy_impl(std::true_type /* Options::is_stoppable*/) {
                layout_.template get<stop_source_type>().request_stop();
            };

            void destroy_impl(std::false_type /* Options::is_stoppable*/){};

        public:
            /// Constructor
            /**
             *  This function will construct the state with storage for R.
             *
             *  This is often invalid because we cannot let it create an empty
             *  executor type. Nonetheless, this constructor is still useful for
             *  allocating pointers.
             */
            operation_state() : operation_state(false) {}

            /// Constructor
            /**
             *  This function will construct the state with storage for R.
             *
             *  This is often invalid because we cannot let it create an empty
             *  executor type. Nonetheless, this constructor is still useful for
             *  allocating pointers.
             */
            explicit operation_state(bool is_deferred)
                : operation_state_base(is_deferred)
                , layout_() {}

            /// Copy constructor
            operation_state(operation_state const &) = default;

            /// Move constructor
            operation_state(operation_state &&) noexcept = default;

            /// Copy assignment operator
            operation_state &
            operator=(operation_state const &)
                = default;

            /// Move assignment operator
            operation_state &
            operator=(operation_state &&)
                = default;

            /// Constructor for state with reference to executor
            /**
             * The executor allows us to emplace continuations on the
             * same executor by default.
             */
            explicit operation_state(executor_type const &ex)
                : operation_state(false, ex) {}

            /// Constructor for potentially deferred state with executor
            /**
             * The executor allows us to emplace continuations on the
             * same executor by default.
             */
            explicit operation_state(bool is_deferred, executor_type const &ex)
                : operation_state_base(is_deferred)
                , layout_(
                      ex,
                      continuations_type{},
                      stop_source_type{},
                      storage_type{}) {}

            /**
             * @}
             */

            /**
             * @name Accessors
             * @{
             */

            /// Set the value of the operation state
            /**
             * This function will directly construct the value with the
             * specified arguments.
             *
             * @tparam Args Argument types
             * @param args Arguments
             */
            template <class... Args>
            void
            set_value(Args &&...args) {
                if (this->is_ready()) {
                    throw_exception(promise_already_satisfied{});
                }
                get_storage().set_value(std::forward<Args>(args)...);
                mark_ready_and_continue_impl(
                    mp_bool<Options::is_continuable>{});
            }

        private:
            void
            mark_ready_and_continue_impl(std::true_type /* is_continuable */) {
                this->mark_ready();
                layout_.template get<continuations_type>().request_run();
            }

            void
            mark_ready_and_continue_impl(std::false_type /* is_continuable */) {
                this->mark_ready();
            }

        public:
            /// Set operation state to an exception
            /**
             *  This sets the exception value and marks the operation state as
             *  ready.
             *
             *  If we try to set an exception on a operation state that's ready,
             * we throw an exception.
             */
            void
            set_exception(std::exception_ptr except) {
                mark_exception_and_continue_impl(
                    mp_bool<Options::is_continuable>{},
                    std::move(except));
            }

        private:
            void
            mark_exception_and_continue_impl(
                std::true_type /* is_continuable */,
                std::exception_ptr except) {
                this->mark_exception(std::move(except));
                layout_.template get<continuations_type>().request_run();
            }

            void
            mark_exception_and_continue_impl(
                std::false_type /* is_continuable */,
                std::exception_ptr except) {
                this->mark_exception(std::move(except));
            }

        public:
            /// Set value with a callable and an argument list
            /**
             * Instead of directly setting the value, we can use this function
             * to use a callable that will later set the value.
             *
             * This simplifies an important pattern where we call a task to
             * set the operation state instead of replicating it in all
             * launching functions, such as @ref async and @ref schedule.
             *
             * @tparam Fn Function type
             * @tparam Args Function argument types
             * @param fn Function object
             * @param args Function arguments
             */
            template <typename Fn, typename... Args>
            void
            apply(Fn &&fn, Args &&...args) {
                try {
                    set_value(regular_void_invoke(
                        std::forward<Fn>(fn),
                        [this]() {
                        return get_stop_source_impl(
                            mp_bool<Options::is_stoppable>{});
                        }(),
                        std::forward<Args>(args)...));
                }
                catch (...) {
                    this->set_exception(std::current_exception());
                }
            }

        private:
            auto
            get_stop_source_impl(std::false_type /* is_stoppable */) {
                boost::ignore_unused(this);
                return regular_void{};
            }

            auto
            get_stop_source_impl(std::true_type /* is_stoppable */) {
                return layout_.template get<stop_source_type>().get_token();
            }

        public:
            /// Set value with a callable and a tuple or arguments
            /**
             * This function is a version of apply that accepts a tuple of
             * arguments instead of a variadic list of arguments.
             *
             * This is useful in functions that schedule deferred futures.
             * In this case, the function arguments need to be stored with the
             * callable object.
             *
             * @tparam Fn Function type
             * @tparam Tuple Tuple type
             * @param fn Function object
             * @param targs Function arguments as tuple
             */
            template <typename Fn, typename Tuple>
            void
            apply_tuple(Fn &&fn, Tuple &&targs) {
                return apply_tuple_impl(
                    std::forward<Fn>(fn),
                    std::forward<Tuple>(targs),
                    std::make_index_sequence<std::tuple_size<
                        std::remove_reference_t<Tuple>>::value>{});
            }

            /// Get the value of the operation state
            /**
             * This function waits for the operation state to become ready and
             * returns its value.
             *
             * This function returns `R&` unless this is a operation state to
             * `void`, in which case `std::add_lvalue_reference_t<R>` is also
             * `void`.
             *
             * @return Reference to the state as a reference to R
             */
            std::add_lvalue_reference_t<R>
            get() {
                this->wait();
                if (this->failed()) {
                    this->throw_internal_exception();
                }
                return get_storage().get();
            }

            /**
             * @}
             */

            /**
             * @name Observers
             * @{
             */

            storage_type &
            get_storage() {
                return layout_.template get<storage_type>();
            }

            storage_type const &
            get_storage() const {
                return layout_.template get<storage_type>();
            }

            executor_type &
            get_executor() {
                return layout_.template get<executor_type>();
            }

            executor_type const &
            get_executor() const {
                return layout_.template get<executor_type>();
            }

            continuations_type &
            get_continuations_source() {
                return layout_.template get<continuations_type>();
            }

            continuations_type const &
            get_continuations_source() const {
                return layout_.template get<continuations_type>();
            }

            stop_source_type &
            get_stop_source() {
                return layout_.template get<stop_source_type>();
            }

            stop_source_type const &
            get_stop_source() const {
                return layout_.template get<stop_source_type>();
            }

            /**
             * @}
             */

        private:
            template <typename Fn, typename Tuple, std::size_t... I>
            void
            apply_tuple_impl(Fn &&fn, Tuple &&targs, std::index_sequence<I...>) {
                return apply(
                    std::forward<Fn>(fn),
                    std::get<I>(std::forward<Tuple>(targs))...);
            }
        };

        /// A functor that binds function arguments for deferred futures
        /**
         * This function binds the function arguments to a function, generating
         * a named functor we can use in a deferred shared state. When the
         * function has arguments, this means we can only store this callable in
         * the deferred shared state instead of storing the function and its
         * arguments directly.
         *
         * @tparam Fn Function type
         * @tparam Args Arguments
         */
        template <class Fn, class... Args>
        struct bind_deferred_state_args {
        public:
            template <class OtherFn, class... OtherArgs>
            explicit bind_deferred_state_args(OtherFn &&f, OtherArgs &&...args)
                : fn_(std::forward<OtherFn>(f))
                , args_(std::make_tuple(std::forward<OtherArgs>(args)...)) {}

            FUTURES_DETAIL(decltype(auto))
            operator()() {
                return tuple_apply(std::move(fn_), std::move(args_));
            }

        private:
            Fn fn_;
            std::tuple<std::decay_t<Args>...> args_;
        };

        /// An extension of shared state with storage for a deferred task
        /**
         *  This class provides the same functionality as shared state and
         *  extra storage for a deferred task.
         *
         *  Because futures keep references to a shared state, what this
         *  effectively achieves is type erasing the task type. Otherwise,
         *  we would need embed the task types in the futures.
         *
         *  For instance, this would make it impossible to have vectors of
         *  futures without wrapping tasks into a more limited std::function
         *  before for erasing the task type.
         */
        template <class R, class Options>
        class deferred_operation_state
            : public operation_state<R, Options>
            , private boost::empty_value<typename Options::function_t, 4> {
            // Storage for the deferred function
            using deferred_function_base = boost::
                empty_value<typename Options::function_t, 4>;
            using deferred_function_type = typename deferred_function_base::type;

        public:
            /**
             * @name Constructors
             * @{
             */

            /// Destructor
            ~deferred_operation_state() override = default;

            /// Constructor
            deferred_operation_state()
                : operation_state<R, Options>(true)
                , deferred_function_base() {}

            /// Copy Constructor
            deferred_operation_state(deferred_operation_state const &) = default;

            /// Move Constructor
            deferred_operation_state(deferred_operation_state &&) noexcept
                = default;

            /// Copy Assignment
            deferred_operation_state &
            operator=(deferred_operation_state const &)
                = default;

            /// Move Assignment
            deferred_operation_state &
            operator=(deferred_operation_state &&)
                = default;

            /// Constructor from the deferred function
            /**
             *  Although the function type will be the same as
             *  deferred_function_type most of the time, any function
             * convertible deferred_function_type is also accepted.
             *
             *  This enables us to optionally create special deferred future
             * types that accept std::function or other types that erase
             * callables. In turn, these types can be used in vectors of
             * deferred futures, which are often necessary.
             *
             */
            template <class Fn>
            explicit deferred_operation_state(
                const typename operation_state<R, Options>::executor_type &ex,
                Fn &&f)
                : operation_state<R, Options>(true, ex)
                , deferred_function_base(boost::empty_init, std::forward<Fn>(f)) {
            }

            /// Constructor from the deferred function
            /**
             *  The function accepts other function and args types so
             *  (i) we can forward the variables, and (ii) allow
             compatible
             *  types to be used here. Most of the time, these will be the
             *  same types as Fn and Args.
             */

            /// Constructor from the deferred function and its arguments
            /**
             *  Although the function type will be the same as
             *  deferred_function_type most of the time, any function
             * convertible deferred_function_type is also accepted so that
             * deferred futures can also erase their function types.
             *
             *  The arguments here will also be stored with the function in a
             *  bind_deferred_state_args struct, turning this into a callable
             *  with no arguments.
             *
             */
            template <class Fn, class... Args>
            explicit deferred_operation_state(
                const typename operation_state<R, Options>::executor_type &ex,
                Fn &&f,
                Args &&...args)
                : operation_state<R, Options>(true, ex)
                , deferred_function_base(
                      boost::empty_init,
                      bind_deferred_state_args<Fn, Args...>(
                          std::forward<Fn>(f),
                          std::forward<Args>(args)...)) {}

            /**
             * @}
             */

            /**
             * @name Shared state functions
             * @{
             */

            /// Get the current value from this operation state
            /**
             * We explicitly define that to use the overload from
             * `operation_state<R, Options>` because empty base functions
             * also have their overloads of `get`.
             */
            using operation_state<R, Options>::get;

            /// Post the deferred task to the executor
            /**
             * When we wait for an operation state for the first time, the base
             * class operation_state_base is responsible for calling
             * post_deferred, which posts any deferred task to the executor.
             *
             * This is the only overload of operation_state_base that implements
             * this function. If the state has no executor, the function is
             * dispatched inline from the thread waiting for it.
             */
            void
            post_deferred() override {
                // if this is a continuation, wait for tasks that come before
                post_deferred_impl(mp_bool<Options::is_always_deferred>{});
            }

        private:
            void
            post_deferred_impl(std::true_type /* is_always_deferred */) {
                post_deferred_to_executor_impl(
                    mp_bool<Options::has_executor>{});
            }

            void
            post_deferred_to_executor_impl(std::true_type /* has_executor */) {
                execute(operation_state<R, Options>::get_executor(), [this]() {
                    this->apply(std::move(this->get_function()));
                });
            }

            void
            post_deferred_to_executor_impl(std::false_type /* has_executor */) {
                this->apply(std::move(this->get_function()));
            }

            void
            post_deferred_impl(std::false_type /* is_always_deferred */) {}
        public:
            /// Wait for the parent operation state to be set
            /**
             * When we wait for an operation state for the first time, the base
             * class operation_state_base cannot call `post_deferred` before
             * waiting for the parent task to finish.
             *
             * This function can be used to ensure this happens by checking if
             * the task it encapsulates is `is_future_continue_task`. This
             * `is_future_continue_task` class is the class we always use
             * to represent a callable with the logic for continuations. Since
             * we always use this class, it also indicates this is a deferred
             * continuation and there's a parent operation we should wait for.
             *
             * Deferred states that refer to continuations need to wait for the
             * parent task to finish before invoking the continuation task.
             *
             * This only happens when the operation state is storing a
             * continuation.
             */
            void
            wait_for_parent() override {
                wait_for_parent_impl(
                    is_future_continue_task<deferred_function_type>{});
            }

        private:
            void
            wait_for_parent_impl(std::true_type /* is_future_continue_task */) {
                this->get_function().before_.wait();
            }

            void
            wait_for_parent_impl(
                std::false_type /* is_future_continue_task */) {}
        public:
            /// Swap two deferred operation states
            void
            swap(deferred_operation_state &other) {
                std::swap(
                    static_cast<operation_state<R, Options> &>(*this),
                    static_cast<operation_state<R, Options> &>(other));
                std::swap(
                    static_cast<deferred_function_base &>(*this),
                    static_cast<deferred_function_base &>(other));
            }

            /**
             * @}
             */

            /**
             * @name Observers
             * @{
             */

            deferred_function_type &
            get_function() {
                return deferred_function_base::get();
            }

            deferred_function_type const &
            get_function() const {
                return deferred_function_base::get();
            }

            /**
             * @}
             */
        };

        /// Check if type is an operation state
        template <typename>
        struct is_operation_state : std::false_type {};

        template <typename... Args>
        struct is_operation_state<operation_state<Args...>> : std::true_type {};

        template <typename... Args>
        struct is_operation_state<deferred_operation_state<Args...>>
            : std::true_type {};

        template <class T>
        constexpr bool is_operation_state_v = is_operation_state<T>::value;

        template <class T>
        struct operation_state_options {};

        template <class R, class Options>
        struct operation_state_options<operation_state<R, Options>> {
            using type = Options;
        };

        template <class R, class Options>
        struct operation_state_options<deferred_operation_state<R, Options>> {
            using type = Options;
        };

        template <class T>
        using operation_state_options_t = typename operation_state_options<
            T>::type;

        /** @} */ // @addtogroup futures Futures
    }             // namespace detail
} // namespace futures

#endif // FUTURES_DETAIL_OPERATION_STATE_HPP
