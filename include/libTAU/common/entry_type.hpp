/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_ENTRY_TYPE_HPP
#define LIBTAU_ENTRY_TYPE_HPP

#include <utility>

#include "libTAU/aux_/export.hpp"
#include "libTAU/blockchain/block.hpp"
#include "libTAU/blockchain/vote.hpp"
#include "libTAU/communication/message.hpp"
#include "libTAU/entry.hpp"

namespace libTAU::common {

    // TODO:: tlv
    const std::string protocol_type = "pid";
    const std::string protocol_payload = "p";
    const std::string entry_type = "t";
    const std::string entry_value = "v";
    const std::string entry_chain_id = "i";

    struct TORRENT_EXPORT entry_task {

        entry_task(const dht::public_key &mPeer, entry mEntry, int64_t mTimestamp) : m_peer(mPeer),
                                                                                            m_entry(std::move(mEntry)),
                                                                                            m_timestamp(mTimestamp) {}

        entry_task(int64_t mDataTypeId, entry mEntry, int64_t mTimestamp) : m_data_type_id(mDataTypeId),
                                                                                   m_entry(std::move(mEntry)),
                                                                                   m_timestamp(mTimestamp) {}

        entry_task(int64_t mDataTypeId, const dht::public_key &mPeer, entry mEntry, int64_t mTimestamp)
                : m_data_type_id(mDataTypeId), m_peer(mPeer), m_entry(std::move(mEntry)), m_timestamp(mTimestamp) {}

        entry_task(int64_t mDataTypeId, const dht::public_key &mPeer, int64_t mTimestamp) : m_data_type_id(mDataTypeId),
                                                                                            m_peer(mPeer),
                                                                                            m_timestamp(mTimestamp) {}

        bool operator<(const entry_task &rhs) const {
            if (m_timestamp < rhs.m_timestamp)
                return true;
            if (m_timestamp > rhs.m_timestamp)
                return false;

            if (m_data_type_id < rhs.m_data_type_id)
                return true;
            if (m_data_type_id > rhs.m_data_type_id)
                return false;

            if (m_peer < rhs.m_peer)
                return true;
            if (m_peer > rhs.m_peer)
                return false;

            std::string encode;
            bencode(std::back_inserter(encode), m_entry);
            std::string rhs_encode;
            bencode(std::back_inserter(encode), rhs.m_entry);
            if (encode < rhs_encode)
                return true;
            if (encode > rhs_encode)
                return false;

            return false;
        }

        bool operator>(const entry_task &rhs) const {
            return rhs < *this;
        }

        bool operator<=(const entry_task &rhs) const {
            return !(rhs < *this);
        }

        bool operator>=(const entry_task &rhs) const {
            return !(*this < rhs);
        }

        std::int64_t m_data_type_id;

        dht::public_key m_peer;

        entry m_entry;

        std::int64_t m_timestamp;
    };

    struct TORRENT_EXPORT communication_entries {
        // data type id
        static const std::int64_t protocol_id = 0;

        communication_entries() = default;

        // @param Construct with entry
        explicit communication_entries(const entry& e);

        communication_entries(std::vector<entry> mEntries) : m_entries(std::move(mEntries)) {}

        void push_back(const entry& e) { m_entries.push_back(e); }

        void pop_back() { m_entries.pop_back(); }

        // @returns the corresponding entry
        entry get_entry() const;

        std::vector<entry> m_entries;
    };

    struct TORRENT_EXPORT blockchain_entries {
        // data type id
        static const std::int64_t protocol_id = 1;

        blockchain_entries() = default;

        // @param Construct with entry
        explicit blockchain_entries(const entry& e);

        blockchain_entries(std::vector<entry> mEntries) : m_entries(std::move(mEntries)) {}

        void push_back(const entry& e) { m_entries.push_back(e); }

        void pop_back() { m_entries.pop_back(); }

        // @returns the corresponding entry
        entry get_entry() const;

        std::vector<entry> m_entries;
    };

    struct TORRENT_EXPORT message_entry {
        // data type id
        static const std::int64_t data_type_id = 0;

        // @param Construct with entry
        explicit message_entry(const entry& e);

        explicit message_entry(communication::message mMsg) : m_msg(std::move(mMsg)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        communication::message m_msg;
    };

    struct TORRENT_EXPORT message_levenshtein_array_entry {
        // data type id
        static const std::int64_t data_type_id = 1;

        // @param Construct with entry
        explicit message_levenshtein_array_entry(const entry& e);

        explicit message_levenshtein_array_entry(aux::bytes mLevenshteinArray) : m_levenshtein_array(std::move(mLevenshteinArray)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        // bytes consist of first byte of ordered messages hash
        aux::bytes m_levenshtein_array;
    };

    struct TORRENT_EXPORT friend_info_request_entry {
        // data type id
        static const std::int64_t data_type_id = 2;

        friend_info_request_entry() = default;

        // @returns the corresponding entry
        entry get_entry() const;
    };

    struct TORRENT_EXPORT friend_info_entry {
        // data type id
        static const std::int64_t data_type_id = 3;

        // @param Construct with entry
        explicit friend_info_entry(const entry& e);

        explicit friend_info_entry(aux::bytes mFriendInfo) : m_friend_info(std::move(mFriendInfo)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        aux::bytes m_friend_info;
    };



    struct TORRENT_EXPORT block_request_entry {
        // data type id
        static const std::int64_t data_type_id = 4;

        // @param Construct with entry
        explicit block_request_entry(const entry& e);

        explicit block_request_entry(const sha256_hash &mHash) : m_hash(mHash) {}

        // @returns the corresponding entry
        entry get_entry() const;

        sha256_hash m_hash;
    };

    struct TORRENT_EXPORT block_entry {
        // data type id
        static const std::int64_t data_type_id = 5;

        // @param Construct with entry
        explicit block_entry(const entry& e);

        explicit block_entry(blockchain::block mBlk) : m_blk(std::move(mBlk)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        blockchain::block m_blk;
    };

    struct TORRENT_EXPORT transaction_request_entry {
        // data type id
        static const std::int64_t data_type_id = 6;

        // @param Construct with entry
        explicit transaction_request_entry(const entry& e);

        explicit transaction_request_entry(const sha256_hash &mHash) : m_hash(mHash) {}

        // @returns the corresponding entry
        entry get_entry() const;

        sha256_hash m_hash;
    };

    struct TORRENT_EXPORT transaction_entry {
        // data type id
        static const std::int64_t data_type_id = 7;

        // @param Construct with entry
        explicit transaction_entry(const entry& e);

        explicit transaction_entry(blockchain::transaction mTx) : m_tx(std::move(mTx)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        blockchain::transaction m_tx;
    };

    struct TORRENT_EXPORT vote_request_entry {
        // data type id
        static const std::int64_t data_type_id = 8;

        // @param Construct with entry
        explicit vote_request_entry(const entry& e);

        explicit vote_request_entry(aux::bytes mChainId) : m_chain_id(std::move(mChainId)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        // chain id
        aux::bytes m_chain_id;
    };

    struct TORRENT_EXPORT vote_entry {
        // data type id
        static const std::int64_t data_type_id = 9;

        // @param Construct with entry
        explicit vote_entry(const entry& e);

        vote_entry(aux::bytes mChainId, const blockchain::vote &mVote) : m_chain_id(std::move(mChainId)), m_vote(mVote) {}

        // @returns the corresponding entry
        entry get_entry() const;

        // chain id
        aux::bytes m_chain_id;

        blockchain::vote m_vote;
    };

    struct TORRENT_EXPORT head_block_request_entry {
        // data type id
        static const std::int64_t data_type_id = 10;

        // @param Construct with entry
        explicit head_block_request_entry(const entry& e);

        explicit head_block_request_entry(aux::bytes mChainId) : m_chain_id(std::move(mChainId)) {}

        // @returns the corresponding entry
        entry get_entry() const;

        // chain id
        aux::bytes m_chain_id;
    };

}


#endif //LIBTAU_ENTRY_TYPE_HPP
