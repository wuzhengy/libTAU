/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_ACCOUNT_HPP
#define LIBTAU_ACCOUNT_HPP

#include "libTAU/aux_/common.h"
#include "libTAU/aux_/export.hpp"
#include "libTAU/entry.hpp"
#include "libTAU/bencode.hpp"
#include "libTAU/bdecode.hpp"

namespace libTAU {
    namespace blockchain {
        class TORRENT_EXPORT account {
        public:
			account() = default;

            explicit account(const entry& e);

            // @param Construct with bencode
            explicit account(std::string encode): account(bdecode(encode)) {}

            account(int64_t mBalance, int64_t mNonce, int64_t mNoteTimestamp, int64_t mBlockNumber) : m_balance(
                    mBalance), m_nonce(mNonce), m_note_timestamp(mNoteTimestamp), m_block_number(mBlockNumber) {}

            account(int64_t mBalance, int64_t mNonce, int64_t mNoteTimestamp, int64_t mEffectivePower,
                    int64_t mBlockNumber) : m_balance(mBalance), m_nonce(mNonce), m_note_timestamp(mNoteTimestamp),
                                            m_effective_power(mEffectivePower), m_block_number(mBlockNumber) {}

            bool empty() const { return m_balance == 0 && m_nonce == 0; }

            int64_t balance() const { return m_balance; }

//            void set_balance(int64_t mBalance) { m_balance = mBalance; }

            int64_t nonce() const { return m_nonce; }

            int64_t note_timestamp() const { return m_note_timestamp; }

            void set_effective_power(int64_t mEffectivePower) { m_effective_power = mEffectivePower; }

            int64_t effective_power() const { return m_effective_power; }

            int64_t block_number() const { return m_block_number; }

            // @returns the corresponding entry
            entry get_entry() const;

        private:

            // populate message data from entry
            void populate(const entry& e);

            // balance
            std::int64_t m_balance{};

            // nonce
            std::int64_t m_nonce{};

            // note timestamp
            std::int64_t m_note_timestamp{};

            // effective power
            std::int64_t m_effective_power{};

            // block number
            std::int64_t m_block_number{};
        };
    }
}


#endif //LIBTAU_ACCOUNT_HPP
