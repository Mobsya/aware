///////////////////////////////////////////////////////////////////////////////
//
// http://github.com/breese/aware
//
// Copyright (C) 2014 Bjorn Reese <breese@users.sourceforge.net>
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
///////////////////////////////////////////////////////////////////////////////

#include <aware/avahi/service.hpp>

namespace aware
{
namespace avahi
{

boost::asio::execution_context::id service::id;

service::service(boost::asio::io_context& io)
    : boost::asio::execution_context::service(io),
      poller(io),
      client(poller)
{
}

} // namespace avahi
} // namespace aware
