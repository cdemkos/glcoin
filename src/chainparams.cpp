// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Copyright (c) 2026 GLCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// =====================================================================
// GLCoin - Custom Chain Parameters
// =====================================================================

#include <chainparams.h>
#include <chainparamsbase.h>
#include <common/args.h>
#include <consensus/params.h>
#include <deploymentinfo.h>
#include <logging.h>
#include <tinyformat.h>
#include <util/chaintype.h>
#include <util/strencodings.h>
#include <util/string.h>

#include <arith_uint256.h>
#include <consensus/merkle.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using util::SplitString;

// -----------------------------------------------------------------------
// Helper: build the genesis coinbase + block
// -----------------------------------------------------------------------
static CBlock CreateGenesisBlock(const char* pszTimestamp,
                                 const CScript& genesisOutputScript,
                                 uint32_t nTime,
                                 uint32_t nNonce,
                                 uint32_t nBits,
                                 int32_t  nVersion,
                                 const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;  // modern Bitcoin Core: field is "version", not "nVersion"
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript()
        << 486604799
        << CScriptNum(4)
        << std::vector<unsigned char>(
               reinterpret_cast<const unsigned char*>(pszTimestamp),
               reinterpret_cast<const unsigned char*>(pszTimestamp) + strlen(pszTimestamp));
    txNew.vout[0].nValue       = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

// -----------------------------------------------------------------------

void ReadSigNetArgs(const ArgsManager& args, CChainParams::SigNetOptions& options)
{
    if (!args.GetArgs("-signetseednode").empty()) {
        options.seeds.emplace(args.GetArgs("-signetseednode"));
    }
    if (!args.GetArgs("-signetchallenge").empty()) {
        const auto signet_challenge = args.GetArgs("-signetchallenge");
        if (signet_challenge.size() != 1) {
            throw std::runtime_error("-signetchallenge cannot be multiple values.");
        }
        const auto val{TryParseHex<uint8_t>(signet_challenge[0])};
        if (!val) {
            throw std::runtime_error(strprintf("-signetchallenge must be hex, not '%s'.", signet_challenge[0]));
        }
        options.challenge.emplace(*val);
    }
}

void ReadRegTestArgs(const ArgsManager& args, CChainParams::RegTestOptions& options)
{
    if (auto value = args.GetBoolArg("-fastprune")) options.fastprune = *value;
    if (HasTestOption(args, "bip94")) options.enforce_bip94 = true;

    for (const std::string& arg : args.GetArgs("-testactivationheight")) {
        const auto found{arg.find('@')};
        if (found == std::string::npos) {
            throw std::runtime_error(strprintf("Invalid format (%s) for -testactivationheight=name@height.", arg));
        }
        const auto value{arg.substr(found + 1)};
        const auto height{ToIntegral<int32_t>(value)};
        if (!height || *height < 0 || *height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Invalid height value (%s) for -testactivationheight=name@height.", arg));
        }
        const auto deployment_name{arg.substr(0, found)};
        if (const auto buried_deployment = GetBuriedDeployment(deployment_name)) {
            options.activation_heights[*buried_deployment] = *height;
        } else {
            throw std::runtime_error(strprintf("Invalid name (%s) for -testactivationheight=name@height.", arg));
        }
    }

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams = SplitString(strDeployment, ':');
        if (vDeploymentParams.size() < 3 || 4 < vDeploymentParams.size()) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end[:min_activation_height]");
        }
        CChainParams::VersionBitsParameters vbparams{};
        const auto start_time{ToIntegral<int64_t>(vDeploymentParams[1])};
        if (!start_time) throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        vbparams.start_time = *start_time;
        const auto timeout{ToIntegral<int64_t>(vDeploymentParams[2])};
        if (!timeout) throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        vbparams.timeout = *timeout;
        if (vDeploymentParams.size() >= 4) {
            const auto min_activation_height{ToIntegral<int64_t>(vDeploymentParams[3])};
            if (!min_activation_height) throw std::runtime_error(strprintf("Invalid min_activation_height (%s)", vDeploymentParams[3]));
            vbparams.min_activation_height = *min_activation_height;
        } else {
            vbparams.min_activation_height = 0;
        }
        bool found = false;
        for (int j = 0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                options.version_bits_parameters[Consensus::DeploymentPos(j)] = vbparams;
                found = true;
                LogInfo("Setting version bits activation parameters for %s to start=%ld, timeout=%ld, min_activation_height=%d",
                    vDeploymentParams[0], vbparams.start_time, vbparams.timeout, vbparams.min_activation_height);
                break;
            }
        }
        if (!found) throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
    }
}

// -----------------------------------------------------------------------
// Shared genesis params
// -----------------------------------------------------------------------
static const char* GENESIS_TIMESTAMP =
    "GLCoin Genesis 2026-04-16 \xe2\x80\x94 Built for the community";
static const CScript GENESIS_SCRIPT =
    CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f")
              << OP_CHECKSIG;

// -----------------------------------------------------------------------
// GLCoin Main Network
// -----------------------------------------------------------------------
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height  = 1;
        consensus.BIP34Hash    = uint256{};
        consensus.BIP65Height  = 1;
        consensus.BIP66Height  = 1;
        consensus.CSVHeight    = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit     = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
        consensus.nPowTargetSpacing  = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94     = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period    = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815;

        genesis = CreateGenesisBlock(GENESIS_TIMESTAMP, GENESIS_SCRIPT,
            1744761600, 1589838484, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
            uint256{"00000000fa4f57a0d6968567b7e73642a338f363c004cd05583e1d53be24ed5f"});

        pchMessageStart[0] = 0xa1; pchMessageStart[1] = 0xb2;
        pchMessageStart[2] = 0xc3; pchMessageStart[3] = 0xd4;
        nDefaultPort = 8555;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 38);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 5);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        bech32_hrp = "glc";

        // ----------------------------------------------------------------
        // CHECKPOINT FIX: open src/chainparams.h and search for the word
        // "checkpoint" — use whatever protected member name you find there.
        // Common names across Bitcoin Core versions:
        //   checkpointData     (most versions up to ~29.x)
        //   m_checkpoints      (some forks)
        // Replace the line below with the correct name if it fails.
        // ----------------------------------------------------------------
        checkpointData = {};

        m_assumeutxo_data = {};
        chainTxData = ChainTxData{1744761600, 0, 0.0};
    }
};

// -----------------------------------------------------------------------
// GLCoin Testnet
// -----------------------------------------------------------------------
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height  = 1;  consensus.BIP34Hash    = uint256{};
        consensus.BIP65Height  = 1;  consensus.BIP66Height  = 1;
        consensus.CSVHeight    = 1;  consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit     = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
        consensus.nPowTargetSpacing  = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94     = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period    = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512;

        genesis = CreateGenesisBlock(GENESIS_TIMESTAMP, GENESIS_SCRIPT,
            1744761600, 0, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock =
            uint256{"00000000fa4f57a0d6968567b7e73642a338f363c004cd05583e1d53be24ed5f"};

        pchMessageStart[0] = 0xb1; pchMessageStart[1] = 0xc2;
        pchMessageStart[2] = 0xd3; pchMessageStart[3] = 0xe4;
        nDefaultPort = 18555;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        bech32_hrp = "tglc";

        checkpointData = {};  // see note in CMainParams above
        m_assumeutxo_data = {};
        chainTxData = ChainTxData{1744761600, 0, 0.0};
    }
};

// -----------------------------------------------------------------------
// GLCoin RegTest
// -----------------------------------------------------------------------
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const RegTestOptions& opts) {
        m_chain_type = ChainType::REGTEST;

        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height  = 1;  consensus.BIP34Hash    = uint256{};
        consensus.BIP65Height  = 1;  consensus.BIP66Height  = 1;
        consensus.CSVHeight    = 1;  consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit     = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
        consensus.nPowTargetSpacing  = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94     = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period    = 144;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
                case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:      consensus.SegwitHeight = int{height}; break;
                case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:  consensus.BIP34Height  = int{height}; break;
                case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:        consensus.BIP65Height  = int{height}; break;
                case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:      consensus.BIP66Height  = int{height}; break;
                case Consensus::BuriedDeployment::DEPLOYMENT_CSV:         consensus.CSVHeight    = int{height}; break;
            }
        }
        for (const auto& [dep, params] : opts.version_bits_parameters) {
            consensus.vDeployments[dep].nStartTime            = params.start_time;
            consensus.vDeployments[dep].nTimeout              = params.timeout;
            consensus.vDeployments[dep].min_activation_height = params.min_activation_height;
        }

        genesis = CreateGenesisBlock(GENESIS_TIMESTAMP, GENESIS_SCRIPT,
            1744761600, 0, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        pchMessageStart[0] = 0xfa; pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5; pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        bech32_hrp = "rglc";

        checkpointData = {};  // see note in CMainParams above
        m_assumeutxo_data = {};
        chainTxData = ChainTxData{0, 0, 0.0};
    }
};

// -----------------------------------------------------------------------
// Factory helpers
// -----------------------------------------------------------------------
static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams& Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, const ChainType chain)
{
    switch (chain) {
        case ChainType::MAIN:    return std::make_unique<CMainParams>();
        case ChainType::TESTNET: return std::make_unique<CTestNetParams>();
        case ChainType::TESTNET4:return std::make_unique<CTestNetParams>();
        case ChainType::SIGNET: {
            auto opts = CChainParams::SigNetOptions{};
            ReadSigNetArgs(args, opts);
            return CChainParams::SigNet(opts);
        }
        case ChainType::REGTEST: {
            auto opts = CChainParams::RegTestOptions{};
            ReadRegTestArgs(args, opts);
            return std::make_unique<CRegTestParams>(opts);
        }
    }
    assert(false);
}

void SelectParams(const ChainType chain)
{
    SelectBaseParams(chain);
    globalChainParams = CreateChainParams(gArgs, chain);
}
