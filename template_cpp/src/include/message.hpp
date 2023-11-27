#include "parser.hpp"

struct Msg_Convoy {
    Parser::Host sender;
    unsigned long original_sender;
    Parser::Host receiver;
    unsigned long msg_id;
    std::array<unsigned int, 8> payload;
    bool is_ack;
    bool is_relay;
    public:
    bool operator==( const Msg_Convoy& other ) {
        if (other.is_ack) 
            return sender.ip == other.receiver.ip &&
                    sender.port == other.receiver.port &&
                    receiver.ip == other.sender.ip &&
                    receiver.port == other.sender.port &&
                    msg_id == other.msg_id;
        else
            return sender.ip == other.sender.ip &&
                    sender.port == other.sender.port &&
                    receiver.ip == other.receiver.ip &&
                    receiver.port == other.receiver.port &&
                    msg_id == other.msg_id;
    }

// doesn't really work yet
    friend bool operator<(const Msg_Convoy& l, const Msg_Convoy& r)
    {
        return std::tie(l.sender.id, l.receiver.id, l.msg_id, l.payload)
             < std::tie(r.sender.id, r.receiver.id, r.msg_id, r.payload); // keep the same order
    }

    void msg_convoy_print() {
        std::cout << "-----------------------------------------" << std::endl;
        std::cout << "Sender: " << this->sender.id << std::endl;
        std::cout << "The Original Sender: " << this->original_sender << std::endl;
        std::cout << "Receiver: " << this->receiver.id << std::endl;
        std::cout << "Message Identifier: " << this->msg_id << std::endl;

        std::cout << "This is the payload: " << std::endl;
        for (unsigned int msg : this->payload) {
            std::cout << msg << std::endl;
        }
        
        if (this->is_ack) {
            std::cout << "This message is an acknowledgeement." << std::endl;
        }

        if (this->is_relay) {
            std::cout << "This message is a relay." << std::endl;
        }
        std::cout << "-----------------------------------------" << std::endl;
    }
};

