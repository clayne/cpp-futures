//
// detail/null_mutex.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_NULL_MUTEX_HPP
#define ASIO_DETAIL_NULL_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <futures/detail/bundled/asio/detail/config.hpp>

#include <futures/detail/bundled/asio/detail/noncopyable.hpp>
#include <futures/detail/bundled/asio/detail/scoped_lock.hpp>

#include <futures/detail/bundled/asio/detail/push_options.hpp>

namespace asio {
namespace detail {

class null_mutex
  : private noncopyable
{
public:
  typedef asio::detail::scoped_lock<null_mutex> scoped_lock;

  // Constructor.
  null_mutex()
  {
  }

  // Destructor.
  ~null_mutex()
  {
  }

  // Lock the mutex.
  void lock()
  {
  }

  // Unlock the mutex.
  void unlock()
  {
  }
};

} // namespace detail
} // namespace asio

#include <futures/detail/bundled/asio/detail/pop_options.hpp>

#endif // ASIO_DETAIL_NULL_MUTEX_HPP
