/*
Copyright (c) 2021, TaiXiang Cui
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#include "libTAU/communication/message.hpp"
#include "libTAU/kademlia/item.hpp"
#include "libTAU/bencode.hpp"
#include "libTAU/entry.hpp"

#include <utility>

namespace libTAU {
    namespace communication {

        message::message(entry e) {
            m_entry = std::move(e);

            populate(m_entry);
            bencode(std::back_inserter(m_encode), m_entry);
            m_hash = dht::item_target_id(m_encode);
        }

        message::message(std::int64_t mTimestamp, aux::bytes mSender, aux::bytes mReceiver, aux::bytes mPayload) :
        m_timestamp(mTimestamp), m_sender(std::move(mSender)),
        m_receiver(std::move(mReceiver)), m_payload(std::move(mPayload)) {
            // timestamp
            m_entry["t"] = entry(m_timestamp);
            // sender
            m_entry["s"] = entry(std::string(m_sender.begin(), m_sender.end()));
            // receiver
            m_entry["r"] = entry(std::string(m_receiver.begin(), m_receiver.end()));
            // payload
            m_entry["p"] = entry(std::string(m_payload.begin(), m_payload.end()));

            bencode(std::back_inserter(m_encode), m_entry);
            m_hash = dht::item_target_id(m_encode);
        }

        void message::populate(const entry &e) {
            // timestamp
            if (auto* i = const_cast<entry *>(e.find_key("t")))
            {
                m_timestamp = i->integer();
            }
            // sender
            if (auto* i = const_cast<entry *>(e.find_key("s")))
            {
                auto sender = i->string();
                m_sender = aux::bytes(sender.begin(), sender.end());
            }
            // receiver
            if (auto* i = const_cast<entry *>(e.find_key("r")))
            {
                auto receiver = i->string();
                m_receiver = aux::bytes(receiver.begin(), receiver.end());
            }
            // payload
            if (auto* i = const_cast<entry *>(e.find_key("p")))
            {
                auto payload = i->string();
                m_payload = aux::bytes(payload.begin(), payload.end());
            }
        }

        std::string message::to_string() const {
            std::ostringstream os;
            os << *this;
            return os.str();
        }

        std::ostream &operator<<(std::ostream &os, const message &message) {
            os << "message m_hash: " << aux::toHex(message.m_hash.to_string()) << "m_timestamp: "
               << message.m_timestamp << " m_sender: " << aux::toHex(message.m_sender) << " m_receiver: "
               << aux::toHex(message.m_receiver) << " m_payload: " << aux::toHex(message.m_payload);
            return os;
        }
    }
}
