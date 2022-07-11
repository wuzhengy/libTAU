/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_STATE_ARRAY_HPP
#define LIBTAU_STATE_ARRAY_HPP

#include <utility>

#include "libTAU/aux_/common_data.h"
#include "libTAU/blockchain/account.hpp"
#include "libTAU/kademlia/item.hpp"
#include "libTAU/sha1_hash.hpp"

namespace libTAU {
    namespace blockchain {
        class state_array {
        public:
            // @param Construct with entry
            explicit state_array(const entry& e);

            // @param Construct with bencode
            explicit state_array(std::string encode): state_array(bdecode(encode)) {}

            explicit state_array(std::vector<account> mStateArray) : m_state_array(std::move(mStateArray)) {
                auto encode = get_encode();
                m_hash = hasher(encode).final();
            }

            const std::vector<account> &StateArray() const { return m_state_array; }

            // @returns the SHA256 hash of this block
            const sha1_hash &sha256() const { return m_hash; }

            entry get_entry() const;

            std::string get_encode() const;

            // @returns a pretty-printed string representation of block structure
            std::string to_string() const;

            friend std::ostream &operator<<(std::ostream &os, const state_array &stateArray);

        private:
            // populate state array from entry
            void populate(const entry& e);

            std::vector<account> m_state_array;

            // sha256 hash
            sha1_hash m_hash;
        };
    }
}


#endif //LIBTAU_STATE_ARRAY_HPP
