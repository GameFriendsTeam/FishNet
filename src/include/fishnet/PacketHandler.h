#pragma once

/*
 * FishNet Packet Handler
 *
 * Defines callback types used throughout FishNet:
 *   - PacketCallback: called when a user-level packet arrives
 *     (raw data, length, sender address)
 *
 * Users set these callbacks on FishServer/FishClient to
 * receive decoded payloads after the RakNet layer strips
 * frame headers, handles reliability, and reassembles splits.
 */

#include "fishnet/utils/Address.h"
#include <cstdint>
#include <cstddef>
#include <functional>

namespace fishnet {

// Callback: raw data, length, sender address
using PacketCallback = std::function<void(const uint8_t* data, size_t len, const Address& sender)>;

} // namespace fishnet
