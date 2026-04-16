// Copyright (c) 2026 GLCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <ipfslink.h>
#include <rpc/server.h>
#include <util/fs.h>

// Call once from AppInitMain() to open the IPFS link database.
void InitIPFSLinkDB(const fs::path& datadir, bool in_memory = false);

// Returns the global DB pointer (may be nullptr before Init is called).
CIPFSLinkDB* GetIPFSLinkDB();

// Register all IPFS RPC commands with the RPC table.
void RegisterIPFSLinkRPCCommands(CRPCTable& t);
