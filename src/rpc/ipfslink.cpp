// Copyright (c) 2026 GLCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// ============================================================
// GLCoin RPC commands for IPFS link storage
//
// storeipfslink  <txid> <cid> [description]
//   Store an IPFS CID anchored to a transaction ID.
//   Returns the stored record as JSON.
//
// getipfslink  <txid>
//   Retrieve the IPFS link record for a given txid.
//
// listipfslinks
//   List all stored IPFS link records, newest first.
//
// removeipfslink  <txid>
//   Delete the IPFS link record for a given txid.
// ============================================================

#include <rpc/ipfslink.h>

#include <ipfslink.h>
#include <node/context.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <univalue.h>
#include <util/strencodings.h>

#include <memory>
#include <stdexcept>

// Global DB instance — initialised by AppInitMain() via InitIPFSLinkDB()
static std::unique_ptr<CIPFSLinkDB> g_ipfslink_db;

void InitIPFSLinkDB(const fs::path& datadir, bool in_memory)
{
    g_ipfslink_db = std::make_unique<CIPFSLinkDB>(
        datadir / "ipfslinks", 1 << 20, in_memory);
}

CIPFSLinkDB* GetIPFSLinkDB()
{
    return g_ipfslink_db.get();
}

// ---------------------------------------------------------------------------
// Helper — serialise a CIPFSLink to UniValue JSON
// ---------------------------------------------------------------------------
static UniValue LinkToJSON(const CIPFSLink& link)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("txid",        link.txid.ToString());
    obj.pushKV("cid",         link.cid);
    obj.pushKV("description", link.description);
    obj.pushKV("timestamp",   link.timestamp);
    return obj;
}

// ---------------------------------------------------------------------------
// storeipfslink
// ---------------------------------------------------------------------------
static RPCHelpMan storeipfslink()
{
    return RPCHelpMan{
        "storeipfslink",
        "Store an IPFS Content Identifier (CID) anchored to a GLCoin transaction.\n"
        "The record is kept in a local LevelDB database and is NOT broadcast to peers.",
        {
            {"txid",        RPCArg::Type::STR_HEX, RPCArg::Optional::NO,
             "The transaction ID to anchor the IPFS link to."},
            {"cid",         RPCArg::Type::STR,     RPCArg::Optional::NO,
             "IPFS Content Identifier (CIDv0 starting with Qm, or CIDv1 starting with bafy/b)."},
            {"description", RPCArg::Type::STR,     RPCArg::Default{std::string{}},
             "Optional human-readable label (max 256 characters)."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR,     "txid",        "Transaction ID"},
                {RPCResult::Type::STR,     "cid",         "IPFS CID"},
                {RPCResult::Type::STR,     "description", "Label"},
                {RPCResult::Type::NUM,     "timestamp",   "Unix timestamp of storage"},
            },
        },
        RPCExamples{
            HelpExampleCli("storeipfslink",
                "\"<txid>\" "
                "\"QmYwAPJzv5CZsnA625s3Xf2nemtYgPpHdWEz79ojWnPbdG\" "
                "\"My document\"")
            + HelpExampleRpc("storeipfslink",
                "\"<txid>\", "
                "\"QmYwAPJzv5CZsnA625s3Xf2nemtYgPpHdWEz79ojWnPbdG\", "
                "\"My document\"")
        },
        [](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!g_ipfslink_db) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "IPFS link database not initialised");
            }

            const uint256 txid = ParseHashV(request.params[0], "txid");
            const std::string cid = request.params[1].get_str();
            const std::string desc = request.params.size() > 2
                ? request.params[2].get_str() : "";

            if (!IsValidIPFSCID(cid)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "Invalid IPFS CID. Expected CIDv0 (Qm…, 46 chars) "
                    "or CIDv1 (bafy…/b…, ≥50 chars).");
            }

            if (!g_ipfslink_db->StoreLinkForTx(txid, cid, desc)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "Failed to store link — record may already exist for this txid.");
            }

            const auto stored = g_ipfslink_db->GetLinkForTx(txid);
            if (!stored) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Store succeeded but read-back failed.");
            }
            return LinkToJSON(*stored);
        },
    };
}

// ---------------------------------------------------------------------------
// getipfslink
// ---------------------------------------------------------------------------
static RPCHelpMan getipfslink()
{
    return RPCHelpMan{
        "getipfslink",
        "Retrieve the IPFS link record associated with a transaction ID.",
        {
            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO,
             "The transaction ID to look up."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "txid",        "Transaction ID"},
                {RPCResult::Type::STR, "cid",         "IPFS CID"},
                {RPCResult::Type::STR, "description", "Label"},
                {RPCResult::Type::NUM, "timestamp",   "Unix timestamp of storage"},
            },
        },
        RPCExamples{
            HelpExampleCli("getipfslink", "\"<txid>\"")
            + HelpExampleRpc("getipfslink", "\"<txid>\"")
        },
        [](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!g_ipfslink_db) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "IPFS link database not initialised");
            }

            const uint256 txid = ParseHashV(request.params[0], "txid");
            const auto link = g_ipfslink_db->GetLinkForTx(txid);
            if (!link) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                    "No IPFS link found for txid " + txid.ToString());
            }
            return LinkToJSON(*link);
        },
    };
}

// ---------------------------------------------------------------------------
// listipfslinks
// ---------------------------------------------------------------------------
static RPCHelpMan listipfslinks()
{
    return RPCHelpMan{
        "listipfslinks",
        "List all stored IPFS link records, newest first.",
        {},
        RPCResult{
            RPCResult::Type::ARR, "", "",
            {
                {RPCResult::Type::OBJ, "", "",
                 {
                     {RPCResult::Type::STR, "txid",        "Transaction ID"},
                     {RPCResult::Type::STR, "cid",         "IPFS CID"},
                     {RPCResult::Type::STR, "description", "Label"},
                     {RPCResult::Type::NUM, "timestamp",   "Unix timestamp"},
                 }},
            },
        },
        RPCExamples{
            HelpExampleCli("listipfslinks", "")
            + HelpExampleRpc("listipfslinks", "")
        },
        [](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!g_ipfslink_db) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "IPFS link database not initialised");
            }

            UniValue arr(UniValue::VARR);
            for (const auto& link : g_ipfslink_db->ListLinks()) {
                arr.push_back(LinkToJSON(link));
            }
            return arr;
        },
    };
}

// ---------------------------------------------------------------------------
// removeipfslink
// ---------------------------------------------------------------------------
static RPCHelpMan removeipfslink()
{
    return RPCHelpMan{
        "removeipfslink",
        "Delete the IPFS link record for a given transaction ID.",
        {
            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO,
             "The transaction ID whose link should be removed."},
        },
        RPCResult{
            RPCResult::Type::BOOL, "", "true if the record was found and deleted"
        },
        RPCExamples{
            HelpExampleCli("removeipfslink", "\"<txid>\"")
            + HelpExampleRpc("removeipfslink", "\"<txid>\"")
        },
        [](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            if (!g_ipfslink_db) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "IPFS link database not initialised");
            }

            const uint256 txid = ParseHashV(request.params[0], "txid");
            const bool removed = g_ipfslink_db->RemoveLinkForTx(txid);
            if (!removed) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                    "No IPFS link found for txid " + txid.ToString());
            }
            return UniValue{removed};
        },
    };
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void RegisterIPFSLinkRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"ipfs", &storeipfslink},
        {"ipfs", &getipfslink},
        {"ipfs", &listipfslinks},
        {"ipfs", &removeipfslink},
    };
    for (const auto& c : commands) t.appendCommand(c.category, &c);
}
