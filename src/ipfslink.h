// Copyright (c) 2026 GLCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// ============================================================
// GLCoin — IPFS Link Storage
//
// Allows any GLCoin transaction to carry an IPFS CID (Content
// Identifier) that is stored in a dedicated LevelDB database
// alongside the normal block/chainstate databases.
//
// Key design decisions:
//   • CIDs are stored off-chain in a node-local DB; they are
//     NOT consensus-critical and do NOT affect block validity.
//   • A CID is associated with a txid so callers can look up
//     the on-chain anchor for any stored content.
//   • The module follows the dbwrapper pattern already used by
//     txdb.cpp / blockfilter.cpp in this codebase.
// ============================================================

#pragma once

#include <dbwrapper.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// CIPFSLink — one stored record
// ---------------------------------------------------------------------------
struct CIPFSLink {
    uint256     txid;        // on-chain anchor transaction
    std::string cid;         // IPFS Content Identifier (CIDv0 or CIDv1)
    std::string description; // optional human-readable label (max 256 chars)
    int64_t     timestamp;   // Unix time when the record was stored locally

    // Serialization (compact, same endian conventions as the rest of the node)
    template <typename Stream>
    void Serialize(Stream& s) const {
        s << txid;
        s << cid;
        s << description;
        s << timestamp;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> txid;
        s >> cid;
        s >> description;
        s >> timestamp;
    }
};

// ---------------------------------------------------------------------------
// CIPFSLinkDB — LevelDB-backed store
// ---------------------------------------------------------------------------
class CIPFSLinkDB : public CDBWrapper
{
public:
    // Opens (or creates) the ipfslink LevelDB at <datadir>/ipfslinks/.
    // cache_size_bytes: LevelDB block cache in bytes (default 1 MiB).
    explicit CIPFSLinkDB(const fs::path& path,
                         size_t cache_size_bytes = 1 << 20,
                         bool in_memory = false);

    // Store a new link. Returns false if the cid is already present for
    // this txid (idempotent — call again with a different txid to add
    // the same CID anchored to multiple transactions).
    bool StoreLinkForTx(const uint256& txid,
                        const std::string& cid,
                        const std::string& description = "");

    // Retrieve the link record for a given txid.
    // Returns std::nullopt if no record exists.
    std::optional<CIPFSLink> GetLinkForTx(const uint256& txid) const;

    // Return all stored links, newest first.
    std::vector<CIPFSLink> ListLinks() const;

    // Delete the record for a txid. Returns true if it existed.
    bool RemoveLinkForTx(const uint256& txid);

private:
    // DB key prefix — avoids collisions if the DB is ever shared.
    static constexpr char KEY_PREFIX = 'I';
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Validate that a string looks like a plausible IPFS CID.
// Accepts CIDv0 (Qm…, 46 chars) and CIDv1 (bafy…, ≥50 chars).
bool IsValidIPFSCID(const std::string& cid);
