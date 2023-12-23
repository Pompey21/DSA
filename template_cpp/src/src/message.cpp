#include "message.hpp"

Message *create_message(unsigned long source_id, unsigned long seq_no, void *data, message_type type, 
                        in_addr_t ip, unsigned short port, int proposal_number, agreement_type agreement,
                        unsigned int size, unsigned int round) {

    int message_size = size == 0 ? static_cast<int>(sizeof(Message)) : static_cast<int>(offsetof(Message, content) + size);
    Message *message = reinterpret_cast<Message *>(malloc(message_size));
    if (message == NULL) return NULL;
    message->source_id = source_id;
    message->seq_no = seq_no;
    if (data != NULL && size > 0) {
        memcpy(&(message->content), data, size);
    }
    message->type = type;
    message->ip = ip;
    message->proposal_number = proposal_number;
    message->port = port;
    message->agreement = agreement;
    message->content_size = size;
    message->round = round;

    return message;
}

std::string ipReadable(in_addr_t ip) {
    in_addr tmp_ip;
    tmp_ip.s_addr = ip;
    return std::string(inet_ntoa(tmp_ip));
}

void to_string(Message *message) {
    std::string type;
    std::string agreement;
    switch (static_cast<int>(message->type)) {
        case 0:
            type = "ACK";
            break;
        case 1:
            type = "SYN";
            break;
        case 2:
            type = "RSYN";
            break;
        case 3:
            type = "BROADCAST";
            break;
        default:
            type = "Invalid";
    }

    switch (static_cast<int>(message->agreement)) {
        case 0:
            agreement = "PROPOSAL";
            break;
        case 1:
            agreement = "ACK";
            break;
        case 2:
            agreement = "NACK";
            break;
        default:
            agreement = "Invalid";
    }

    std::cout << "{" << std::endl << std::flush;
    std::cout << "  ip: " << message->ip << std::endl << std::flush;
    std::cout << "  port: " << message->port << std::endl << std::flush;
    std::cout << "  size_content:" << message->content_size << std::endl << std::flush;
    std::cout << "  content_addr: " << message->content << std::endl << std::flush;
    std::cout << "  seq_no: " << message->seq_no << std::endl << std::flush;
    std::cout << "  source_id: " << message->source_id << std::endl << std::flush;
    std::cout << "  message_type: " << type << std::endl << std::flush;
    std::cout << "  agreement_type: " << agreement << std::endl << std::flush;
    std::cout << "}" << std::endl << std::flush;
}

unsigned short portReadable( unsigned short port) { return ntohs(port); }