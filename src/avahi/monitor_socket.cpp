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

#include <cassert>
#include <queue>
#include <utility> // std::pair
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/asio/placeholders.hpp>
#include <aware/avahi/monitor_socket.hpp>
#include <aware/avahi/detail/browser.hpp>

namespace aware
{

namespace avahi
{

namespace detail
{

class monitor : public browser::listener
{
    typedef std::pair<boost::system::error_code, aware::contact> response_type;
    typedef aware::avahi::monitor_socket::async_listen_handler handler_type;

public:
    monitor(boost::asio::io_service& io,
            aware::contact& contact)
        : contact_pointer(&contact)
    {
        browser = boost::make_shared<detail::browser>(
            boost::asio::use_service<avahi::service>(io).get_client(),
            contact,
            boost::ref(*this));
    }

    //! @pre Must be called from an io_service thread
    void prepare(aware::contact& contact, handler_type handler)
    {
        contact_pointer = &contact;
        handlers.push(handler);
        perform();
    }

    //! @pre Must be called from an io_service thread
    void perform()
    {
        if (responses.empty())
            return; // Nothing to send
        if (handlers.empty())
            return; // No receiver

        const response_type& response = responses.front();
        handler_type& handler = handlers.front();
        *contact_pointer = response.second;
        handler(response.first);
        contact_pointer = 0;
        responses.pop();
        handlers.pop();
    }

    virtual void on_appear(const aware::contact& contact)
    {
        boost::system::error_code success;
        responses.push(std::make_pair(success, contact));
        perform();
    }

    virtual void on_disappear(const aware::contact& contact)
    {
        boost::system::error_code success;
        responses.push(std::make_pair(success, contact));
        perform();
    }

    virtual void on_failure(const boost::system::error_code& error)
    {
        aware::contact no_contact("");
        responses.push(std::make_pair(error, no_contact));
        perform();
    }

private:
    aware::contact *contact_pointer;
    boost::shared_ptr<detail::browser> browser;
    std::queue<response_type> responses;
    std::queue<handler_type> handlers;
};

} // namespace detail

monitor_socket::monitor_socket(boost::asio::io_service& io)
    : boost::asio::basic_io_object<avahi::service>(io)
{
}

void monitor_socket::async_listen(aware::contact& contact,
                                  async_listen_handler handler)
{
    // Perform from io_service thread because the constructor of
    // detail::browser will invoke the first callback
    get_io_service().post(boost::bind(&monitor_socket::do_async_listen,
                                      this,
                                      boost::ref(contact),
                                      handler));
}

void monitor_socket::do_async_listen(aware::contact& contact,
                                     async_listen_handler handler)
{
    const std::string& key = contact.type();
    monitor_map::iterator where = monitors.lower_bound(key);
    if ((where == monitors.end()) || (monitors.key_comp()(key, where->first)))
    {
        where = monitors.insert(
            where,
            monitor_map::value_type(
                key,
                boost::make_shared<aware::avahi::detail::monitor>(
                    boost::ref(get_io_service()),
                    boost::ref(contact))));
    }
    assert(where != monitors.end());
    where->second->prepare(contact, handler);
}

} // namespace avahi
} // namespace aware
