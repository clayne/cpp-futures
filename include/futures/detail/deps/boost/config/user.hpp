//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef FUTURES_DETAIL_DEPS_BOOST_CONFIG_USER_HPP
#define FUTURES_DETAIL_DEPS_BOOST_CONFIG_USER_HPP

#include <futures/config.hpp>

// Include boost/config/user.hpp from external or bundled boost
#if defined(FUTURES_HAS_BOOST)
#    include <boost/config/user.hpp>
#else
#    include <futures/detail/bundled/boost/config/user.hpp>
#endif

#endif // FUTURES_DETAIL_DEPS_BOOST_CONFIG_USER_HPP