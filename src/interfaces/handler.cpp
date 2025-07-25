// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2019-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/handler.h>

#include <util/system.h>

#include <boost/signals2/connection.hpp>
#include <memory>
#include <utility>

namespace interfaces {
namespace {

    class HandlerImpl : public Handler {
    public:
        explicit HandlerImpl(boost::signals2::connection connection)
            : m_connection(std::move(connection)) {}

        void disconnect() override { m_connection.disconnect(); }

        boost::signals2::scoped_connection m_connection;
    };

} // namespace

std::unique_ptr<Handler> MakeHandler(boost::signals2::connection connection) {
    return std::make_unique<HandlerImpl>(std::move(connection));
}

} // namespace interfaces
