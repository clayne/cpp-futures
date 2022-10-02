//
// execution/execute.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2020 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_EXECUTION_EXECUTE_HPP
#define BOOST_ASIO_EXECUTION_EXECUTE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <futures/detail/bundled/boost/asio/detail/config.hpp>
#include <futures/detail/bundled/boost/asio/detail/type_traits.hpp>
#include <futures/detail/bundled/boost/asio/execution/detail/as_invocable.hpp>
#include <futures/detail/bundled/boost/asio/execution/detail/as_receiver.hpp>
#include <futures/detail/bundled/boost/asio/traits/execute_member.hpp>
#include <futures/detail/bundled/boost/asio/traits/execute_free.hpp>

#include <futures/detail/bundled/boost/asio/detail/push_options.hpp>

#if defined(GENERATING_DOCUMENTATION)

namespace boost {
namespace asio {
namespace execution {

/// A customisation point that executes a function on an executor.
/**
 * The name <tt>execution::execute</tt> denotes a customisation point object.
 *
 * For some subexpressions <tt>e</tt> and <tt>f</tt>, let <tt>E</tt> be a type
 * such that <tt>decltype((e))</tt> is <tt>E</tt> and let <tt>F</tt> be a type
 * such that <tt>decltype((f))</tt> is <tt>F</tt>. The expression
 * <tt>execution::execute(e, f)</tt> is ill-formed if <tt>F</tt> does not model
 * <tt>invocable</tt>, or if <tt>E</tt> does not model either <tt>executor</tt>
 * or <tt>sender</tt>. Otherwise, it is expression-equivalent to:
 *
 * @li <tt>e.execute(f)</tt>, if that expression is valid. If the function
 *   selected does not execute the function object <tt>f</tt> on the executor
 *   <tt>e</tt>, the program is ill-formed with no diagnostic required.
 *
 * @li Otherwise, <tt>execute(e, f)</tt>, if that expression is valid, with
 *   overload resolution performed in a context that includes the declaration
 *   <tt>void execute();</tt> and that does not include a declaration of
 *   <tt>execution::execute</tt>. If the function selected by overload
 *   resolution does not execute the function object <tt>f</tt> on the executor
 *   <tt>e</tt>, the program is ill-formed with no diagnostic required.
 */
inline constexpr unspecified execute = unspecified;

/// A type trait that determines whether a @c execute expression is well-formed.
/**
 * Class template @c can_execute is a trait that is derived from
 * @c true_type if the expression <tt>execution::execute(std::declval<T>(),
 * std::declval<F>())</tt> is well formed; otherwise @c false_type.
 */
template <typename T, typename F>
struct can_execute :
  integral_constant<bool, automatically_determined>
{
};

} // namespace execution
} // namespace asio
} // namespace boost

#else // defined(GENERATING_DOCUMENTATION)

namespace boost {
namespace asio {
namespace execution {

template <typename T, typename R>
struct is_sender_to;

namespace detail {

template <typename S, typename R>
void submit_helper(BOOST_ASIO_MOVE_ARG(S) s, BOOST_ASIO_MOVE_ARG(R) r);

} // namespace detail
} // namespace execution
} // namespace asio
} // namespace boost
namespace asio_execution_execute_fn {

using boost::asio::conditional;
using boost::asio::decay;
using boost::asio::declval;
using boost::asio::enable_if;
using boost::asio::execution::detail::as_receiver;
using boost::asio::execution::detail::is_as_invocable;
using boost::asio::execution::is_sender_to;
using boost::asio::false_type;
using boost::asio::result_of;
using boost::asio::traits::execute_free;
using boost::asio::traits::execute_member;
using boost::asio::true_type;

void execute();

enum overload_type
{
  call_member,
  call_free,
  adapter,
  ill_formed
};

template <typename T, typename F, typename = void>
struct call_traits
{
  BOOST_ASIO_STATIC_CONSTEXPR(overload_type, overload = ill_formed);
};

template <typename T, typename F>
struct call_traits<T, void(F),
  typename enable_if<
    (
      execute_member<T, F>::is_valid
    )
  >::type> :
  execute_member<T, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(overload_type, overload = call_member);
};

template <typename T, typename F>
struct call_traits<T, void(F),
  typename enable_if<
    (
      !execute_member<T, F>::is_valid
      &&
      execute_free<T, F>::is_valid
    )
  >::type> :
  execute_free<T, F>
{
  BOOST_ASIO_STATIC_CONSTEXPR(overload_type, overload = call_free);
};

template <typename T, typename F>
struct call_traits<T, void(F),
  typename enable_if<
    (
      !execute_member<T, F>::is_valid
      &&
      !execute_free<T, F>::is_valid
      &&
      conditional<true, true_type,
       typename result_of<typename decay<F>::type&()>::type
      >::type::value
      &&
      conditional<
        !is_as_invocable<
          typename decay<F>::type
        >::value,
        is_sender_to<
          T,
          as_receiver<typename decay<F>::type, T>
        >,
        false_type
      >::type::value
    )
  >::type>
{
  BOOST_ASIO_STATIC_CONSTEXPR(overload_type, overload = adapter);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

struct impl
{
  template <typename T, typename F>
  BOOST_ASIO_CONSTEXPR typename enable_if<
    call_traits<T, void(F)>::overload == call_member,
    typename call_traits<T, void(F)>::result_type
  >::type
  operator()(
      BOOST_ASIO_MOVE_ARG(T) t,
      BOOST_ASIO_MOVE_ARG(F) f) const
    BOOST_ASIO_NOEXCEPT_IF((
      call_traits<T, void(F)>::is_noexcept))
  {
    return BOOST_ASIO_MOVE_CAST(T)(t).execute(BOOST_ASIO_MOVE_CAST(F)(f));
  }

  template <typename T, typename F>
  BOOST_ASIO_CONSTEXPR typename enable_if<
    call_traits<T, void(F)>::overload == call_free,
    typename call_traits<T, void(F)>::result_type
  >::type
  operator()(
      BOOST_ASIO_MOVE_ARG(T) t,
      BOOST_ASIO_MOVE_ARG(F) f) const
    BOOST_ASIO_NOEXCEPT_IF((
      call_traits<T, void(F)>::is_noexcept))
  {
    return execute(BOOST_ASIO_MOVE_CAST(T)(t), BOOST_ASIO_MOVE_CAST(F)(f));
  }

  template <typename T, typename F>
  BOOST_ASIO_CONSTEXPR typename enable_if<
    call_traits<T, void(F)>::overload == adapter,
    typename call_traits<T, void(F)>::result_type
  >::type
  operator()(
      BOOST_ASIO_MOVE_ARG(T) t,
      BOOST_ASIO_MOVE_ARG(F) f) const
    BOOST_ASIO_NOEXCEPT_IF((
      call_traits<T, void(F)>::is_noexcept))
  {
    return boost::asio::execution::detail::submit_helper(
        BOOST_ASIO_MOVE_CAST(T)(t),
        as_receiver<typename decay<F>::type, T>(
          BOOST_ASIO_MOVE_CAST(F)(f), 0));
  }
};

template <typename T = impl>
struct static_instance
{
  static const T instance;
};

template <typename T>
const T static_instance<T>::instance = {};

} // namespace asio_execution_execute_fn
namespace boost {
namespace asio {
namespace execution {
namespace {

static BOOST_ASIO_CONSTEXPR const asio_execution_execute_fn::impl&
  execute = asio_execution_execute_fn::static_instance<>::instance;

} // namespace

template <typename T, typename F>
struct can_execute :
  integral_constant<bool,
    asio_execution_execute_fn::call_traits<T, void(F)>::overload !=
      asio_execution_execute_fn::ill_formed>
{
};

#if defined(BOOST_ASIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename F>
constexpr bool can_execute_v = can_execute<T, F>::value;

#endif // defined(BOOST_ASIO_HAS_VARIABLE_TEMPLATES)

} // namespace execution
} // namespace asio
} // namespace boost

#endif // defined(GENERATING_DOCUMENTATION)

#include <futures/detail/bundled/boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_EXECUTION_EXECUTE_HPP
