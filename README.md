# GLCoin

> A Bitcoin Core-based cryptocurrency node implementation — full validation, wallet support, and peer-to-peer networking.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](COPYING)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/Build-CMake%203.22%2B-blue.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)]()

---

## Table of Contents

- [About](#about)
- [Features](#features)
- [Repository Structure](#repository-structure)
- [Build Instructions](#build-instructions)
  - [Prerequisites](#prerequisites)
  - [Linux](#linux)
  - [macOS](#macos)
  - [Windows (MSVC)](#windows-msvc)
  - [Windows (MinGW)](#windows-mingw)
- [Executables](#executables)
- [Configuration](#configuration)
- [Testing](#testing)
- [Development](#development)
- [Contributing](#contributing)
- [Security](#security)
- [License](#license)

---

## About

**GLCoin** is a full-node cryptocurrency implementation built on the Bitcoin Core codebase. It connects to the GLCoin peer-to-peer network to download and fully validate blocks and transactions. An optional wallet and graphical user interface are also available.

The `master` branch is regularly built and tested but is not guaranteed to be completely stable. Stable release versions are tagged from release branches.

---

## Features

- Full peer-to-peer node with complete block and transaction validation
- Built-in wallet with SQLite backend (optional)
- Graphical user interface via Qt 6 (optional)
- JSON-RPC and REST API interfaces
- ZeroMQ notification support (optional)
- External signer support (e.g. hardware wallets)
- Tor, I2P, and CJDNS network privacy support
- USDT tracepoints for userspace tracing (optional)
- Multiprocess architecture (`bitcoin-node` / `bitcoin-gui`) on Linux/macOS
- Experimental `libbitcoinkernel` library for embedding consensus logic

---

## Repository Structure

```
glcoin/
├── src/              # Core source code (node, wallet, consensus, RPC, P2P)
├── test/             # Functional and unit tests
├── doc/              # Documentation (build guides, developer notes, RPC docs)
├── contrib/          # Auxiliary scripts and tools
├── depends/          # Dependency build system
├── cmake/            # CMake modules and configuration
├── ci/               # Continuous Integration scripts
├── share/            # Desktop integration files
├── CMakeLists.txt    # Top-level build definition
└── CMakePresets.json # Preset configurations for common build targets
```

---

## Build Instructions

### Prerequisites

| Dependency   | Minimum Version | Notes                          |
|--------------|-----------------|--------------------------------|
| CMake        | 3.22            | Build system                   |
| C++ Compiler | C++20 capable   | GCC 11+, Clang 14+, MSVC 2022 |
| Python       | 3.10            | Required for functional tests  |
| SQLite3      | 3.7.17          | Required for wallet support    |
| libevent     | 2.1.8           | Required for networking        |
| Boost        | —               | Required (auto-detected)       |

Optional dependencies: `Qt 6.2+` (GUI), `ZeroMQ 4.0+` (ZMQ), `QRencode` (QR codes), `libmultiprocess` (IPC).

---

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake python3 \
    libsqlite3-dev libevent-dev libboost-dev

# Clone and build
git clone https://github.com/cdemkos/glcoin.git
cd glcoin
cmake -B build
cmake --build build -j$(nproc)
```

For a detailed guide, see [doc/build-unix.md](doc/build-unix.md) or [doc/INSTALL_linux.md](doc/INSTALL_linux.md).

---

### macOS

```bash
# Install dependencies via Homebrew
brew install cmake python sqlite libevent boost

# Clone and build
git clone https://github.com/cdemkos/glcoin.git
cd glcoin
cmake -B build
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

For a detailed guide, see [doc/build-osx.md](doc/build-osx.md).

---

### Windows (MSVC)

Requires Visual Studio 2022 with C++ workload and CMake support.

```powershell
git clone https://github.com/cdemkos/glcoin.git
cd glcoin
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

For a detailed guide, see [doc/build-windows-msvc.md](doc/build-windows-msvc.md).

---

### Windows (MinGW / Cross-compile)

See [doc/build-windows.md](doc/build-windows.md) for cross-compilation instructions using MinGW.

---

## Executables

The following binaries are produced by the build:

| Executable              | Description                                              | Default |
|-------------------------|----------------------------------------------------------|---------|
| `glcoin`                | Combined node + wallet binary                            | ON      |
| `glcoind`               | Background daemon (headless node)                        | ON      |
| `glcoin-cli`            | RPC command-line client                                  | ON      |
| `glcoin-qt`             | Graphical user interface (Qt)                            | OFF     |
| `glcoin-tx`             | Transaction creation/manipulation utility                | ON*     |
| `glcoin-util`           | General-purpose utility                                  | ON*     |
| `glcoin-wallet`         | Wallet management tool                                   | ON*     |
| `glcoin-node`           | Multiprocess node binary (Linux/macOS)                   | ON*     |
| `glcoin-gui`            | Multiprocess GUI binary (Linux/macOS)                    | OFF     |
| `glcoin-chainstate`     | Experimental chainstate utility                          | OFF     |
| `libbitcoinkernel`      | Experimental embeddable consensus library                | OFF     |

*Enabled when `BUILD_TESTS=ON` (default).

To enable the GUI:

```bash
cmake -B build -DBUILD_GUI=ON
cmake --build build
```

---

## Configuration

GLCoin reads its configuration from `glcoin.conf` located in the data directory:

- **Linux/macOS:** `~/.glcoin/glcoin.conf`
- **Windows:** `%APPDATA%\GLCoin\glcoin.conf`

See [doc/bitcoin-conf.md](doc/bitcoin-conf.md) for a full reference of available options.

**Example `glcoin.conf`:**

```ini
# Network
listen=1
maxconnections=40

# RPC
server=1
rpcuser=yourusername
rpcpassword=yourpassword

# Performance
dbcache=512
```

---

## Testing

### Unit Tests

Unit tests are compiled alongside the main build and run with `ctest`:

```bash
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

See [src/test/README.md](src/test/README.md) for more details.

### Functional Tests

Python-based regression and integration tests:

```bash
# Run all functional tests
build/test/functional/test_runner.py

# Run a specific test
build/test/functional/test_runner.py feature_rbf.py
```

Test dependencies must be installed — see [test/README.md](test/README.md).

### Benchmarks

```bash
cmake -B build -DBUILD_BENCH=ON
cmake --build build
build/src/bench/bench_glcoin
```

See [doc/benchmarking.md](doc/benchmarking.md) for details.

---

## Development

- Developer guidelines and coding standards: [doc/developer-notes.md](doc/developer-notes.md)
- Contribution workflow: [CONTRIBUTING.md](CONTRIBUTING.md)
- Implemented BIPs: [doc/bips.md](doc/bips.md)
- JSON-RPC interface: [doc/JSON-RPC-interface.md](doc/JSON-RPC-interface.md)
- REST interface: [doc/REST-interface.md](doc/REST-interface.md)
- ZeroMQ notifications: [doc/zmq.md](doc/zmq.md)
- Wallet management: [doc/managing-wallets.md](doc/managing-wallets.md)
- Output descriptors: [doc/descriptors.md](doc/descriptors.md)
- PSBT support: [doc/psbt.md](doc/psbt.md)
- Tor support: [doc/tor.md](doc/tor.md)
- I2P support: [doc/i2p.md](doc/i2p.md)
- Fuzzing: [doc/fuzzing.md](doc/fuzzing.md)
- USDT tracing: [doc/tracing.md](doc/tracing.md)

---

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests. Useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

**Note:** Translation changes are not accepted as GitHub pull requests. Please submit translations via the project's Transifex page. See [doc/translation_process.md](doc/translation_process.md) for details.

---

## Security

For responsible disclosure of security vulnerabilities, please refer to [SECURITY.md](SECURITY.md).

---

## License

GLCoin is released under the terms of the **MIT License**. See [COPYING](COPYING) for more information or visit https://opensource.org/license/MIT.

Copyright (c) 2026 The GLCoin developers
