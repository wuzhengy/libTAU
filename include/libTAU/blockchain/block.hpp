/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_BLOCK_HPP
#define LIBTAU_BLOCK_HPP

#include <utility>

#include "libTAU/aux_/common.h"
#include "libTAU/aux_/common_data.h"
#include "libTAU/blockchain/transaction.hpp"
#include "libTAU/entry.hpp"
#include "libTAU/bencode.hpp"
#include "libTAU/bdecode.hpp"

namespace libTAU::blockchain {

    enum block_version {
        block_version1,
        block_unknown_version,
    };

    class TORRENT_EXPORT block {
    public:
        block() = default;

        // @param Construct with entry
        explicit block(entry e);

        // @param Construct with bencode
        explicit block(std::string encode): block(bdecode(encode)) {}

        block(block_version mVersion, aux::bytes mChainId, int64_t mTimestamp, int64_t mBlockNumber,
              aux::bytes mPreviousBlockRoot, int64_t mBaseTarget, int64_t mCumulativeDifficulty,
              aux::bytes mGenerationSignature, transaction mTx, aux::bytes mMiner,
              int64_t mMinerBalance, int64_t mMinerNonce, int64_t mSenderBalance, int64_t mSenderNonce,
              int64_t mReceiverBalance, int64_t mReceiverNonce, aux::bytes mSignature) :
              m_version(mVersion), m_chain_id(std::move(mChainId)), m_timestamp(mTimestamp), m_block_number(mBlockNumber),
              m_previous_block_root(std::move(mPreviousBlockRoot)), m_base_target(mBaseTarget),
              m_cumulative_difficulty(mCumulativeDifficulty), m_generation_signature(std::move(mGenerationSignature)),
              m_tx(std::move(mTx)), m_miner(std::move(mMiner)), m_miner_balance(mMinerBalance), m_miner_nonce(mMinerNonce),
              m_sender_balance(mSenderBalance), m_sender_nonce(mSenderNonce), m_receiver_balance(mReceiverBalance),
              m_receiver_nonce(mReceiverNonce), m_signature(std::move(mSignature)) {}

        block_version version() const { return m_version; }

        const aux::bytes &chain_id() const { return m_chain_id; }

        int64_t timestamp() const { return m_timestamp; }

        int64_t block_number() const { return m_block_number; }

        const aux::bytes &previous_block_root() const { return m_previous_block_root; }

        int64_t base_target() const { return m_base_target; }

        int64_t cumulative_difficulty() const { return m_cumulative_difficulty; }

        const aux::bytes &generation_signature() const { return m_generation_signature; }

        const transaction &tx() const { return m_tx; }

        const aux::bytes &miner() const { return m_miner; }

        int64_t miner_balance() const { return m_miner_balance; }

        int64_t miner_nonce() const { return m_miner_nonce; }

        int64_t sender_balance() const { return m_sender_balance; }

        int64_t sender_nonce() const { return m_sender_nonce; }

        int64_t receiver_balance() const { return m_receiver_balance; }

        int64_t receiver_nonce() const { return m_receiver_nonce; }

        const aux::bytes &signature() const { return m_signature; }

    private:

        // populate block data from entry
        void populate(const entry& e);

        // block version
        block_version m_version;

        // chain id
        aux::bytes m_chain_id;

        // timestamp
        std::int64_t m_timestamp;

        // block number
        std::int64_t m_block_number;

        // previous block root
        aux::bytes m_previous_block_root;

        // base target
        std::int64_t m_base_target;

        // cumulative difficulty
        std::int64_t m_cumulative_difficulty;

        // generation signature
        aux::bytes m_generation_signature;

        // tx
        transaction m_tx;

        // miner
        aux::bytes m_miner;

        // miner balance
        std::int64_t m_miner_balance;

        // miner nonce
        std::int64_t m_miner_nonce;

        // sender balance
        std::int64_t m_sender_balance;

        // sender nonce
        std::int64_t m_sender_nonce;

        // receiver balance
        std::int64_t m_receiver_balance;

        // receiver nonce
        std::int64_t m_receiver_nonce;

        // signature
        aux::bytes m_signature;
    };
}


#endif //LIBTAU_BLOCK_HPP
