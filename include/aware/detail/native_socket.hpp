#ifndef AWARE_DETAIL_NATIVE_SOCKET_HPP
#define AWARE_DETAIL_NATIVE_SOCKET_HPP

///////////////////////////////////////////////////////////////////////////////
//
// http://github.com/breese/aware
//
// Copyright (C) 2013 Bjorn Reese <breese@users.sourceforge.net>
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/function.hpp>

namespace aware {
namespace detail {

    template <typename Service>
    class non_closing_service : public Service {
    public:
        non_closing_service(boost::asio::io_context& io) : Service(io) {}

        void destroy(typename Service::implementation_type&) {
            // Do not close the file descriptor as we have no ownership of it
        }
    };

    //! @brief Boost.Asio wrapper for a native socket
    class native_socket {
        using socket_type = boost::asio::basic_stream_socket<boost::asio::ip::tcp>;

    public:
        typedef socket_type::native_handle_type native_handle_type;

        native_socket(boost::asio::io_context&, native_handle_type);
        ~native_socket();

        template <typename Handler>
        void async_read_event(BOOST_ASIO_MOVE_ARG(Handler) handler) {
            socket.async_wait(socket_type::wait_read, handler);
        }

        template <typename Handler>
        void async_write_event(BOOST_ASIO_MOVE_ARG(Handler) handler) {
            socket.async_wait(socket_type::wait_write, handler);
        }

        native_handle_type native_handle();

    private:
        socket_type socket;
    };

}  // namespace detail
}  // namespace aware

#endif  // AWARE_DETAIL_NATIVE_SOCKET_HPP
