/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#ifndef LIBTAU_NEW_MSG_SIGNAL_HPP
#define LIBTAU_NEW_MSG_SIGNAL_HPP


#include <libtorrent/aux_/common.h>
#include <libtorrent/aux_/rlp.h>
#include "libtorrent/aux_/export.hpp"

namespace libtorrent {
    namespace communication {

        // The ``new_msg_signal`` class represents
        // one type of mutable wrapper,
        // is used to publish new message info in XY channel
        class TORRENT_EXPORT new_msg_signal {

        public:

            // @param _rlp rlp encode
            new_msg_signal(aux::bytesConstRef _rlp);

            // @returns device id
            aux::bytes device_id() const {
                return m_device_id;
            }

            // @returns hash prefix bytes
            aux::bytes hash_prefix_bytes() const {
                return m_hash_prefix_bytes;
            }

            // @returns timestamp
            uint32_t timestamp() const {
                return m_timestamp;
            }

            // Serialises this new message signal to an RLPStream
            void streamRLP(aux::RLPStream& _s) const;

            // @returns the RLP serialisation of this new message signal
            aux::bytes rlp() const { aux::RLPStream s; streamRLP(s); return s.out(); }

        private:
            // Construct new message signal object from rlp serialisation
            void populate(aux::RLP const& _new_msg_signal);

            // device id
            aux::bytes m_device_id;

            // bytes consist of first byte of ordered messages hash
            aux::bytes m_hash_prefix_bytes;

            // new message signal timestamp
            uint32_t m_timestamp;
        };
    }
}


#endif //LIBTAU_NEW_MSG_SIGNAL_HPP
