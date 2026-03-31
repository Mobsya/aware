///////////////////////////////////////////////////////////////////////////////
//
// http://github.com/breese/aware
//
// Copyright (C) 2016 Bjorn Reese <breese@users.sourceforge.net>
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
///////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <algorithm>
#include <iterator>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/utility/in_place_factory.hpp>
#include <aware/bonjour/error.hpp>
#include <aware/bonjour/detail/throw_on_error.hpp>
#include <aware/bonjour/detail/monitor.hpp>
#include "dns_sd.hpp"

namespace aware {
namespace bonjour {
    namespace detail {

        //-----------------------------------------------------------------------------
        // monitor
        //-----------------------------------------------------------------------------

        monitor::monitor(boost::asio::io_context& io, detail::handle& connection) : io(io), connection(connection) {}

        void monitor::listen(aware::contact& contact, async_listen_handler handler) {
            if(permanent_error) {
                handler(permanent_error);
                return;
            }

            if(!browser) {
                type = contact.type();
                // Browser will continously trigger announcements via its listener
                browser = boost::in_place(contact.type(), boost::ref(connection), boost::ref(*this));
            }
            assert(contact.type() == type);

            resolvers_type::iterator where = resolvers.find(contact.type());
            if(where != resolvers.end()) {
                resolvers.erase(where);
            }

            requests.push(request(boost::ref(contact), handler));
        }

        void monitor::on_browser_appear(const aware::contact& contact, bool commit) {
            scope_container::iterator entry = scopes.lower_bound(contact.name());
            if((entry == scopes.end()) || (scopes.key_comp()(contact.name(), entry->first))) {
                entry = scopes.insert(
                    entry,
                    std::make_pair(contact.name(), boost::make_shared<scope>(boost::ref(io), boost::ref(*this))));
            }
            entry->second->submit_appear(contact);
            entry->second->commit_appear();
        }

        void monitor::on_browser_disappear(const aware::contact& contact, bool commit) {
            scope_container::iterator entry = scopes.find(contact.name());
            if(entry != scopes.end()) {
                entry->second->submit_disappear(contact);
                entry->second->commit_disappear();
            }
        }

        void monitor::on_browser_failure(const boost::system::error_code& error) {
            if(requests.empty()) {
                permanent_error = error;
            } else {
                monitor::request request = requests.front();
                requests.pop();
                request.handler(error);
            }
        }

        void monitor::on_resolver_done(const aware::contact& contact) {
            if(requests.empty()) {
                permanent_error = bonjour::error::make_error_code(kDNSServiceErr_BadState);
                // assert(false);
            } else {
                scope_container::iterator entry = scopes.find(contact.name());
                if(entry != scopes.end()) {
                    entry->second->resolved(contact);
                }
            }
            resolvers.erase(contact);
        }

        void monitor::on_resolver_failure(const boost::system::error_code& error) {
            if(requests.empty()) {
                permanent_error = error;
            } else {
                monitor::request request = requests.front();
                requests.pop();
                request.handler(error);
            }
        }

        //-----------------------------------------------------------------------------
        // monitor::request
        //-----------------------------------------------------------------------------

        monitor::request::request(aware::contact& contact, async_listen_handler handler)
            : contact(contact), handler(handler) {}

        //-----------------------------------------------------------------------------
        // monitor::scope
        //-----------------------------------------------------------------------------

        monitor::scope::scope(boost::asio::io_context& io, monitor& self) : self(self) {}

        monitor::scope::~scope() {
            boost::system::error_code dummy;
        }

        void monitor::scope::submit_appear(const aware::contact& key) {
            additions.insert(key);
        }

        void monitor::scope::submit_disappear(const aware::contact& key) {
            removals.insert(key);
        }

        void monitor::scope::commit_appear() {
            execute_appear();
		}

        void monitor::scope::commit_disappear() {
            execute_disappear();
        }

        void monitor::scope::resolved(const aware::contact& contact) {
            monitor::request request = self.requests.front();
            self.requests.pop();
            assert(request.contact.type() == contact.type());
            request.contact = contact;
            activate(request.contact);
            request.handler(boost::system::error_code());
        }

        void monitor::scope::activate(const aware::contact& key) {
            active.insert(key);
        }

        void monitor::scope::deactivate(const aware::contact& key) {
            active.erase(key);
        }

        void monitor::scope::execute_appear() {
            for(addition_container::const_iterator it = additions.begin(); it != additions.end(); ++it) {
                self.resolvers.insert(std::make_pair(
                    *it, boost::make_shared<detail::resolver>(boost::ref(self.connection), *it, boost::ref(self))));
                // Continues in resolver listener
            }
            additions.clear();
        }

        void monitor::scope::execute_disappear() {
            removal_container::iterator it = removals.begin();
            while(it != removals.end()) {
                if(self.requests.empty()) {
                    self.permanent_error = bonjour::error::make_error_code(kDNSServiceErr_BadState);
                    //assert(false);
                    break;  // for
                } else {
                    monitor::request request = self.requests.front();
                    self.requests.pop();

                    assert(request.contact.type() == it->type());
                    request.contact =
                        aware::contact(it->type()).name(it->name()).domain(it->domain()).index(it->index());
                    deactivate(request.contact);
                    request.handler(boost::system::error_code());
                    removals.erase(it++);
                }
            }
        }
    }  // namespace detail
}  // namespace bonjour
}  // namespace aware
