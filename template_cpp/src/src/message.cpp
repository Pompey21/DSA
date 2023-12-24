#include "message.hpp"

Message *create_message(
                    unsigned long source_id, 
                    unsigned long sequence_number, 
                    void *data, 
                    message_type type, 
                    in_addr_t ip, 
                    unsigned short port, 
                    int proposal_number, 
                    agreement_type agreement,
                    unsigned int size, 
                    unsigned int round) {

    int message_size = size == 0 ? static_cast<int>(sizeof(Message)) : static_cast<int>(offsetof(Message, content) + size);
    Message *message = reinterpret_cast<Message *>(malloc(message_size));
    if (message == nullptr) {
        return nullptr;
    }

    message->source_id = source_id;
    message->sequence_number = sequence_number;

    if (data != nullptr && size > 0) {
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

std::string ip_to_readable(in_addr_t ip) {
    in_addr tmp_ip;
    tmp_ip.s_addr = ip;
    return std::string(inet_ntoa(tmp_ip));
}

unsigned short port_to_readable( unsigned short port) { 
    return ntohs(port); 
}

std::string message_type_to_readable(message_type msg_typ) {
    if (msg_typ == ACK) {
        return "ACK";
    } else if (msg_typ == SYN) {
        return "SYN";
    } else if (msg_typ == RSYN) {
        return "RSYN";
    } else if (msg_typ == BROADCAST) {
        return "BROADCAST";
    }
    else {
        return "Message Type is not consistent with the decided enum...";
    }
}

std::string ack_status_to_readable(ack_status ack_stat) {
    if (ack_stat == NOT_SEND) {
        return "NOT SEND";
    } else if (ack_stat == DELETED) {
        return "DELETED";
    } else if (ack_stat == NOT_RECEIVED) {
        return "NOT_RECEIVED";
    } else {
        return "Ack Type is not consistent with the decided enum...";
    }
}

std::string agreement_type_to_readable(agreement_type agr_type) {
    if (agr_type == PROPOSAL) {
        return "PROPOSAL";
    } else if (agr_type == ACKNOWLEDGEMENT) {
        return "ACKNOWLEDGEMENT";
    } else if (agr_type == NACK) {
        return "NACK";
    } else {
        return "Agreement Type is not consistent with the decided enum...";
    }
}