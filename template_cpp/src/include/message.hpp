#include "parser.hpp"

struct Msg_Convoy {
    Parser::Host sender;
    unsigned long original_sender;
    Parser::Host receiver;
    unsigned long msg_id;
    std::array<unsigned int, 8> payload;
    bool is_ack;
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
    bool operator<(const Msg_Convoy& other) const {
        std::string message_group_identifier = std::to_string(this->original_sender) + "_" + std::to_string(this->msg_id);
        std::string message_group_identifier_other = std::to_string(other.original_sender) + "_" + std::to_string(other.msg_id);
        return message_group_identifier < message_group_identifier_other;
    }

    void msg_convoy_print() {
        std::cout << "\n-----------------------------------------" << std::endl;
        std::cout << "Sender: " << this->sender.id << std::endl;
        std::cout << "The Original Sender: " << this->original_sender << std::endl;
        std::cout << "Receiver: " << this->receiver.id << std::endl;
        std::cout << "Message Identifier: " << this->msg_id << std::endl;

        std::cout << "This is the payload: " << std::endl;
        for (unsigned int msg : this->payload) {
            std::cout << msg << std::endl;
        }
        
        // if (this->is_ack) {
        //     std::cout << "This message is an acknowledgeement." << std::endl;
        // }
        std::cout << "-----------------------------------------\n" << std::endl;
    }
};

