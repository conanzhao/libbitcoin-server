/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/interface/transaction_pool.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;

static constexpr auto canonical = bc::message::version::level::canonical;

void transaction_pool::fetch_transaction(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto hash = deserial.read_hash();

    // The response allows confirmed and unconfirmed transactions.
    node.chain().fetch_transaction(hash, false,
        std::bind(&transaction_pool::transaction_fetched,
            _1, _2, _3, _4, request, handler));
}

void transaction_pool::transaction_fetched(const code& ec, transaction_ptr tx,
    size_t, size_t, const message& request, send_handler handler)
{
    const auto result = build_chunk(
    {
        message::to_bytes(ec),
        tx->to_data(canonical)
    });

    handler(message(request, result));
}

// Save to tx pool and announce to all connected peers.
// FUTURE: conditionally subscribe to penetration notifications.
void transaction_pool::broadcast(server_node& node, const message& request,
    send_handler handler)
{
    const auto tx = std::make_shared<bc::message::transaction>();

    if (!tx->from_data(canonical, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // Organize into our chain.
    tx->validation.simulate = false;

    // This call is async but blocks on other organizations until started.
    // Subscribed channels will pick up and announce via tx inventory to peers.
    node.chain().organize(tx,
        std::bind(handle_broadcast, _1, request, handler));
}

void transaction_pool::handle_broadcast(const code& ec, const message& request,
    send_handler handler)
{
    // Returns validation error or error::success.
    handler(message(request, ec));
}

void transaction_pool::validate2(server_node& node, const message& request,
    send_handler handler)
{
    const auto tx = std::make_shared<bc::message::transaction>();

    if (!tx->from_data(canonical, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // Simulate organization into our chain.
    tx->validation.simulate = true;

    // This call is async but blocks on other organizations until started.
    node.chain().organize(tx,
        std::bind(handle_validated2, _1, request, handler));
}

void transaction_pool::handle_validated2(const code& ec,
    const message& request, send_handler handler)
{
    // Returns validation error or error::success.
    handler(message(request, ec));
}

} // namespace server
} // namespace libbitcoin
