//
// is_executor.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2020 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_IS_EXECUTOR_HPP
#define BOOST_ASIO_IS_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <futures/detail/bundled/boost/asio/detail/config.hpp>
#include <futures/detail/bundled/boost/asio/detail/is_executor.hpp>

#include <futures/detail/bundled/boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {

/// The is_executor trait detects whether a type T meets the Executor type
/// requirements.
/**
 * Class template @c is_executor is a UnaryTypeTrait that is derived from @c
 * true_type if the type @c T meets the syntactic requirements for Executor,
 * otherwise @c false_type.
 */
template <typename T>
struct is_executor
#if defined(GENERATING_DOCUMENTATION)
  : integral_constant<bool, automatically_determined>
#else // defined(GENERATING_DOCUMENTATION)
  : boost::asio::detail::is_executor<T>
#endif // defined(GENERATING_DOCUMENTATION)
{
};

} // namespace asio
} // namespace boost

#include <futures/detail/bundled/boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_IS_EXECUTOR_HPP
