IPFS Link Storage Release Notes
================================

New Feature: IPFS Link Storage (`CIPFSLinkDB`)
-----------------------------------------------

GLCoin now ships an **IPFS Link Storage** module that allows any on-chain
transaction to be associated with an IPFS Content Identifier (CID). The
association is stored in a dedicated node-local LevelDB database
(`<datadir>/ipfslinks/`) and does **not** affect block validity or consensus.

### Overview

- **File:** `src/ipfslink.cpp` / `src/ipfslink.h`
- **Documentation:** `doc/ipfslink.md`
- **Pattern:** Follows the `dbwrapper` pattern used by `txdb.cpp` and
  `blockfilter.cpp`; `CIPFSLinkDB` extends `CDBWrapper` directly.
- **Off-chain design:** CIDs are stored node-locally. They are
  NOT consensus-critical and do NOT affect block validity.

### Data Record (`CIPFSLink`)

Each stored record contains:

| Field         | Type           | Description                                              |
|---------------|----------------|----------------------------------------------------------|
| `txid`        | `uint256`      | On-chain anchor transaction ID                           |
| `cid`         | `std::string`  | IPFS Content Identifier (CIDv0 or CIDv1)                 |
| `description` | `std::string`  | Optional human-readable label (max 256 characters)       |
| `timestamp`   | `int64_t`      | Unix time when the record was stored locally             |

Records are serialised using the same endian conventions as the rest of
the node and stored under a 33-byte key: `'I'` (1 byte) + txid (32 bytes).

### Public API (`CIPFSLinkDB`)

| Method               | Behaviour                                                                                     |
|----------------------|-----------------------------------------------------------------------------------------------|
| `StoreLinkForTx`     | Validates CID format and description length, checks idempotency (will not overwrite an existing record), serialises and writes to LevelDB. Returns `false` on any failure. |
| `GetLinkForTx`       | Looks up the record by txid; returns `std::nullopt` if not found.                             |
| `ListLinks`          | Iterates all stored links, skips corrupt entries, and returns results sorted newest-first.    |
| `RemoveLinkForTx`    | Erases the record for a given txid; returns `true` only if the record existed.               |

### CID Validation (`IsValidIPFSCID`)

A standalone helper validates that a string is a plausible IPFS CID:

- **CIDv0:** starts with `Qm`, exactly 46 characters, full base58btc charset check.
- **CIDv1:** minimum 50 characters, first character is `b`, `B`, `z`, or `f`
  (plausible multibase prefix).

### Design Decisions

- **DB key layout:** `'I'` prefix + 32-byte txid = 33 bytes total. The prefix
  avoids collisions if the database is ever shared with other modules.
- **Idempotent writes:** `StoreLinkForTx` returns `false` if a record already
  exists for the given txid; existing records are never overwritten.
- **Description cap:** enforced at write time (max 256 characters) before any
  DB access.
- **Corrupt-entry resilience:** `ListLinks` wraps each deserialisation in a
  `try/catch`; bad records are silently skipped without aborting the scan.
- **Newest-first ordering:** `ListLinks` applies `std::sort` with a timestamp
  comparator after full iteration.

### New RPCs

See `src/rpc/ipfslink.cpp` for the JSON-RPC surface exposed by this module.

### Low-level Changes

- `src/ipfslink.h` — `CIPFSLink` struct and `CIPFSLinkDB` class declaration;
  `IsValidIPFSCID` helper declaration.
- `src/ipfslink.cpp` — full implementation of `CIPFSLinkDB` and
  `IsValidIPFSCID`.
- `doc/ipfslink.md` — user-facing documentation for the IPFS Link module.
