#include "parser.hpp"

class Msg_Convoy {
    public:
        Msg_Convoy(Parser::Host sender, Parser::Host receiver, unsigned long msg_id, std::array<unsigned int, 8> payload, bool is_ack) {
            this->sender = sender;
            this->receiver = receiver;
            this->msg_id = msg_id;
            this->payload = payload;
            this->is_ack = is_ack;
        }

        Msg_Convoy& operator=(const Msg_Convoy & other) {
            this->sender = other.sender;
            this->receiver = other.receiver;
            this->msg_id = other.msg_id;
            this->payload = other.payload;
            this->is_ack = other.is_ack;
            return *this;
        }

        friend bool operator<(const Msg_Convoy& l, const Msg_Convoy& r) {
            return std::tie(l.sender.id, l.receiver.id, l.msg_id, l.payload, l.is_ack)
             < std::tie(r.sender.id, r.receiver.id, r.msg_id, r.payload, r.is_ack); // keep the same order
        };

    private:
    // assignable
        Parser::Host sender;
        Parser::Host receiver;
        unsigned long msg_id;
        std::array<unsigned int, 8> payload;
        bool is_ack;
};