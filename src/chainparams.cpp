// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <common/args.h>
#include <util/chaintype.h>

#include <cassert>
#include <memory>

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams& Params()
{
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, ChainType chain)
{
    switch (chain) {
    case ChainType::MAIN:
        return CChainParams::Main();
    case ChainType::TESTNET:
        return CChainParams::TestNet();
    case ChainType::TESTNET4:
        return CChainParams::TestNet4();
    case ChainType::SIGNET: {
        auto opts = CChainParams::SignNetOptions{};
        if (!args.GetArgs("-signetseednode").empty()) {
            opts.seeds.emplace(args.GetArgs("-signetseednode"));
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
            opts.challenge.emplace(*val);
        }
        return CChainParams::SignNet(opts);
    }
    case ChainType::REGTEST: {
        auto opts = CChainParams::RegTestOptions{};
        if (auto value = args.GetBoolArg("-fastprune")) opts.fastprune = *value;
        if (args.GetBoolArg("-bip94", false)) opts.enforce_bip94 = true;
        return CChainParams::RegTest(opts);
    }
    }
    assert(false);
}

void SelectParams(const ChainType chain)
{
    SelectBaseParams(chain);
    globalChainParams = CreateChainParams(gArgs, chain);
}
