#include "parser.hpp"

class Msg_Convoy {
    public:
        Msg_Convoy(Parser parser, unsigned long msg_id, std::array<unsigned int, 8> payload, bool is_ack);
        Msg_Convoy& operator=(const Msg_Convoy & other);
        Msg_Convoy& operator<(const Msg_Convoy & other);

    private:
    // assignable
        Parser::Host sender;
        Parser::Host receiver;
        unsigned long msg_id;
        std::array<unsigned int, 8> payload;
        bool is_ack;
};