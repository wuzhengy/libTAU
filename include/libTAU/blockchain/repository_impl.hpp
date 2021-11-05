/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_REPOSITORY_IMPL_HPP
#define LIBTAU_REPOSITORY_IMPL_HPP


#include <sqlite3.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include "libTAU/blockchain/repository.hpp"
#include "libTAU/blockchain/repository_track.hpp"

namespace libTAU::blockchain {
    struct repository_impl final : repository {

        repository_impl(sqlite3 *mSqlite, leveldb::DB *mLeveldb) : m_sqlite(mSqlite), m_leveldb(mLeveldb) {}

        bool init() override;

        bool create_peer_db(const aux::bytes &chain_id) override;

        bool delete_peer_db(const aux::bytes &chain_id) override;

        std::set<dht::public_key> get_all_peers(const aux::bytes &chain_id) override;

        dht::public_key get_peer_randomly(const aux::bytes &chain_id) override;

        bool delete_peer_in_peer_db(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

        //        account get_account_from_user_db(aux::bytes chain_id, dht::public_key pubKey) override;

//        bool update_user_state_db(const block &b) override;

        bool add_peer_in_peer_db(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

        bool create_gossip_peer_db(const aux::bytes &chain_id) override;

        bool delete_gossip_peer_db(const aux::bytes &chain_id) override;

        std::set<dht::public_key> get_all_gossip_peers(const aux::bytes &chain_id) override;

        dht::public_key get_gossip_peer_randomly(const aux::bytes &chain_id) override;

        bool delete_peer_in_gossip_peer_db(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

        bool add_peer_in_gossip_peer_db(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

        bool is_account_exist(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

//        std::int64_t get_effective_power(const aux::bytes &chain_id, const dht::public_key &pubKey) override;
//
//        account get_account(const aux::bytes &chain_id, const dht::public_key &pubKey) override;
//
//        account get_account_with_effective_power(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

//        account get_account_without_verification(aux::bytes chain_id, dht::public_key pubKey) override;

        bool is_block_exist(const sha256_hash &hash) override;

        block get_block_by_hash(const sha256_hash &hash) override;

//        block get_main_chain_block_by_number(const aux::bytes &chain_id, std::int64_t block_number) override;

//        sha256_hash get_main_chain_block_hash_by_number(const aux::bytes &chain_id, std::int64_t block_number) override;

//        bool save_block(block b, bool main_chain) override;

//        bool forward_update_state_linker(const aux::bytes &chain_id, const dht::public_key& pubKey,
//                                         state_linker& stateLinker, const sha256_hash &current_block_hash) override;
//
//        bool backward_update_state_linker(const aux::bytes &chain_id, const dht::public_key& pubKey,
//                                          state_linker& stateLinker, const sha256_hash &current_block_hash) override;

        bool connect_tip_block(block &b) override;

        bool connect_tail_block(block &b) override;

        bool rollback_block(block &b) override;

        bool expire_block(block &b) override;

        bool delete_block(const sha256_hash &hash) override;

        sha256_hash get_best_tip_block_hash(const aux::bytes &chain_id) override;

        bool set_best_tip_block_hash(const aux::bytes &chain_id, const sha256_hash &hash) override;

        bool delete_best_tip_block_hash(const aux::bytes &chain_id) override;

        sha256_hash get_best_tail_block_hash(const aux::bytes &chain_id) override;

        bool set_best_tail_block_hash(const aux::bytes &chain_id, const sha256_hash &hash) override;

        bool delete_best_tail_block_hash(const aux::bytes &chain_id) override;

        repository *start_tracking() override;

        void update_batch(const std::map<std::string, std::string> &cache, const std::vector<block> &main_chain_blocks) override;

        bool flush() override;

        bool commit() override;

        void rollback() override;

        account_block_pointer get_account_block_pointer(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

        bool save_account_block_pointer(const aux::bytes &chain_id, const dht::public_key &pubKey, const account_block_pointer &accountBlockPointer) override;

        bool delete_account_block_pointer(const aux::bytes &chain_id, const dht::public_key &pubKey) override;

//        account find_state_from_block(const dht::public_key &pubKey, const block &b) override;

        state_linker get_state_linker(const sha256_hash &block_hash) override;

        bool save_state_linker(const state_linker &stateLinker) override;

        bool delete_state_linker(const sha256_hash &block_hash) override;

        bool save_block(block &b) override;

//        bool save_non_main_chain_block(block &b) override;

        bool delete_index_info(const aux::bytes &chain_id, std::int64_t block_number) override;

        index_key_info get_index_info(const aux::bytes &chain_id, std::int64_t block_number) override;

        bool save_index_info(const aux::bytes &chain_id, std::int64_t block_number, const index_key_info &indexKeyInfo) override;

//        bool delete_expired_data_by_height(const aux::bytes &chain_id, std::int64_t block_number) override;

        std::set<aux::bytes> get_all_chains() override;

        bool save_chains(const std::set<aux::bytes> &chains) override;

//        bool add_new_chain(const aux::bytes &chain_id) override;
//
//        bool delete_chain(const aux::bytes &chain_id) override;

    private:

        // sqlite3 instance
        sqlite3 *m_sqlite;

        // leveldb instance
        leveldb::DB* m_leveldb;

        // leveldb write batch
        leveldb::WriteBatch m_write_batch;

        // main chain blocks
        std::vector<block> m_main_chain_blocks;
    };
}

#endif //LIBTAU_REPOSITORY_IMPL_HPP
