// Copyright (c) 2026 GLCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ipfslink.h>

#include <logging.h>
#include <serialize.h>
#include <streams.h>
#include <util/time.h>

#include <algorithm>
#include <stdexcept>

// ---------------------------------------------------------------------------
// DB key encoding
// ---------------------------------------------------------------------------
// Key layout: [ 'I' (1 byte) | txid (32 bytes) ] = 33 bytes total
static std::vector<uint8_t> MakeKey(const uint256& txid)
{
    std::vector<uint8_t> key;
    key.reserve(33);
    key.push_back(static_cast<uint8_t>('I'));
    const auto* p = txid.begin();
    key.insert(key.end(), p, p + 32);
    return key;
}

// ---------------------------------------------------------------------------
// CIPFSLinkDB
// ---------------------------------------------------------------------------
CIPFSLinkDB::CIPFSLinkDB(const fs::path& path,
                         size_t cache_size_bytes,
                         bool in_memory)
    : CDBWrapper(DBParams{
          .path          = path,
          .cache_bytes   = cache_size_bytes,
          .memory_only   = in_memory,
          .wipe_data     = false,
          .obfuscate     = false,
      })
{
    LogInfo("[IPFSLink] database opened at %s", fs::PathToString(path));
}

bool CIPFSLinkDB::StoreLinkForTx(const uint256& txid,
                                  const std::string& cid,
                                  const std::string& description)
{
    if (!IsValidIPFSCID(cid)) {
        LogInfo("[IPFSLink] StoreLinkForTx: invalid CID '%s'", cid);
        return false;
    }
    if (description.size() > 256) {
        LogInfo("[IPFSLink] StoreLinkForTx: description too long (%zu chars)",
                description.size());
        return false;
    }

    const auto key = MakeKey(txid);

    // Idempotency check — do not overwrite an existing record
    if (Exists(key)) {
        LogInfo("[IPFSLink] StoreLinkForTx: record already exists for txid %s",
                txid.ToString());
        return false;
    }

    CIPFSLink link;
    link.txid        = txid;
    link.cid         = cid;
    link.description = description;
    link.timestamp   = GetTime<std::chrono::seconds>().count();

    DataStream ss{};
    link.Serialize(ss);

    const bool ok = Write(key, MakeByteSpan(ss));
    if (ok) {
        LogInfo("[IPFSLink] stored CID %s for txid %s",
                cid, txid.ToString());
    }
    return ok;
}

std::optional<CIPFSLink> CIPFSLinkDB::GetLinkForTx(const uint256& txid) const
{
    const auto key = MakeKey(txid);
    std::vector<uint8_t> raw;
    if (!Read(key, raw)) return std::nullopt;

    SpanReader sr{0, 0, raw};
    CIPFSLink link;
    try {
        link.Unserialize(sr);
    } catch (const std::exception& e) {
        LogInfo("[IPFSLink] GetLinkForTx: deserialization error: %s", e.what());
        return std::nullopt;
    }
    return link;
}

std::vector<CIPFSLink> CIPFSLinkDB::ListLinks() const
{
    std::vector<CIPFSLink> results;

    // Iterate over all keys that start with the 'I' prefix
    const std::vector<uint8_t> prefix{static_cast<uint8_t>('I')};
    auto it = NewIterator();
    for (it->Seek(prefix); it->Valid(); it->Next()) {
        const auto k = it->GetKey();
        if (k.empty() || k[0] != static_cast<uint8_t>('I')) break;

        const auto v = it->GetValue();
        SpanReader sr{0, 0, v};
        CIPFSLink link;
        try {
            link.Unserialize(sr);
            results.push_back(std::move(link));
        } catch (...) {
            // Skip corrupt entries
        }
    }

    // Sort newest first
    std::sort(results.begin(), results.end(),
              [](const CIPFSLink& a, const CIPFSLink& b) {
                  return a.timestamp > b.timestamp;
              });
    return results;
}

bool CIPFSLinkDB::RemoveLinkForTx(const uint256& txid)
{
    const auto key = MakeKey(txid);
    if (!Exists(key)) return false;
    return Erase(key);
}

// ---------------------------------------------------------------------------
// IsValidIPFSCID
// ---------------------------------------------------------------------------
bool IsValidIPFSCID(const std::string& cid)
{
    if (cid.empty()) return false;

    // CIDv0: starts with "Qm", base58btc, exactly 46 characters
    if (cid.size() == 46 && cid[0] == 'Q' && cid[1] == 'm') {
        // Quick base58 charset check
        static const std::string B58 =
            "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
        for (char c : cid) {
            if (B58.find(c) == std::string::npos) return false;
        }
        return true;
    }

    // CIDv1: starts with "b" (base32lower) or "B" (base32upper) or "z" (base58),
    // minimum 50 characters
    if (cid.size() >= 50) {
        char first = cid[0];
        if (first == 'b' || first == 'B' || first == 'z' || first == 'f') {
            return true; // Accept any plausible CIDv1 multibase prefix
        }
    }

    return false;
}
