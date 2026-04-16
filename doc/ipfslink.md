# GLCoin — IPFS Link Storage

GLCoin ships a built-in module that lets any node operator anchor
an [IPFS](https://ipfs.tech/) Content Identifier (CID) to a GLCoin
transaction and store it locally.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  glcoind / glcoin-qt             │
│                                                 │
│  RPC layer  ──►  src/rpc/ipfslink.cpp           │
│                       │                         │
│                       ▼                         │
│  Storage    ──►  src/ipfslink.cpp               │
│              (CIPFSLinkDB : CDBWrapper)         │
│                       │                         │
│                       ▼                         │
│  Disk       ──►  <datadir>/ipfslinks/           │
│                  (LevelDB)                      │
└─────────────────────────────────────────────────┘
```

- Records are **node-local** — they are not broadcast to peers and
  do not affect block validity or consensus.
- Each record links one `txid` to one IPFS CID.
- CIDs are validated (CIDv0 `Qm…` or CIDv1 `bafy…/b…`).

## RPC Commands

### `storeipfslink <txid> <cid> [description]`

Store an IPFS CID anchored to a transaction.

```bash
glcoin-cli storeipfslink \
  "<txid>" \
  "QmYwAPJzv5CZsnA625s3Xf2nemtYgPpHdWEz79ojWnPbdG" \
  "My whitepaper"
```

Returns:
```json
{
  "txid": "<txid>",
  "cid": "QmYwAPJzv5CZsnA625s3Xf2nemtYgPpHdWEz79ojWnPbdG",
  "description": "My whitepaper",
  "timestamp": 1744761600
}
```

### `getipfslink <txid>`

Retrieve the IPFS link for a transaction.

```bash
glcoin-cli getipfslink "<txid>"
```

### `listipfslinks`

List all stored links, newest first.

```bash
glcoin-cli listipfslinks
```

### `removeipfslink <txid>`

Delete the link record for a transaction.

```bash
glcoin-cli removeipfslink "<txid>"
```

## Integration Checklist

After the files are committed, wire the module into the build and
runtime:

1. **`src/CMakeLists.txt`** — add `ipfslink.cpp` to the
   `bitcoin_node` target sources.
2. **`src/rpc/CMakeLists.txt`** (or the node target) — add
   `rpc/ipfslink.cpp`.
3. **`src/init.cpp`** — call `InitIPFSLinkDB(gArgs.GetDataDirNet())`
   inside `AppInitMain()` after the data directory is ready.
4. **`src/rpc/server.cpp`** (or wherever `RegisterAllCoreRPCCommands`
   lives) — call `RegisterIPFSLinkRPCCommands(tableRPC)`.
5. Rebuild: `cmake --build build -j$(nproc)`
