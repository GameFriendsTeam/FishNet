#pragma once

/*
 * FishNet — Unified Include
 *
 * Single header that pulls in the entire FishNet API.
 * FishNet is a cross-platform RakNet protocol implementation in C++20.
 * It handles UDP networking, reliability, ordering, split packets,
 * connection handshake, keep-alive, and graceful disconnect.
 *
 * Include this file to get: platform abstraction, packet types,
 * binary buffer, address utils, FishPeer engine, FishServer, FishClient.
 */

// Platform
#include "fishnet/platform.h"

// Protocol
#include "fishnet/protocol/Constants.h"
#include "fishnet/protocol/EncapsulatedPacket.h"

// Utilities
#include "fishnet/utils/Address.h"
#include "fishnet/utils/AddressSerialization.h"
#include "fishnet/utils/BinaryBuffer.h"

// Packets
#include "fishnet/packets/UnconnectedPing.h"
#include "fishnet/packets/UnconnectedPong.h"
#include "fishnet/packets/OpenConnectionRequest1.h"
#include "fishnet/packets/OpenConnectionReply1.h"
#include "fishnet/packets/OpenConnectionRequest2.h"
#include "fishnet/packets/OpenConnectionReply2.h"
#include "fishnet/packets/ConnectionRequest.h"
#include "fishnet/packets/ConnectionRequestAccepted.h"
#include "fishnet/packets/NewIncomingConnection.h"
#include "fishnet/packets/ConnectedPing.h"
#include "fishnet/packets/DisconnectionNotification.h"
#include "fishnet/packets/AckNak.h"

// Core
#include "fishnet/Connection.h"
#include "fishnet/PacketHandler.h"
#include "fishnet/FishPeer.h"

// High-level
#include "fishnet/FishServer.h"
#include "fishnet/FishClient.h"
