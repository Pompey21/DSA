#pragma once

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstdlib>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

enum message_type {ACK, SYN, RSYN, BROADCAST};

enum ack_status {NOT_SEND, DELETED, NOT_RECEIVED};

enum agreement_type {PROPOSAL, ACKNOWLEGEMENT, NACK};

class Metadata {
    public:
        unsigned long source_id;
        unsigned long iterm_id;
        unsigned long sequence_number;

        Metadata(unsigned long source_id, unsigned long iterm_id, unsigned long sequence_number) {
            this->source_id = source_id;
            this->iterm_id = iterm_id;
            this->sequence_number = sequence_number;
        }

        ~Metadata() {}
};

typedef struct {
    unsigned long sequence_number;
    unsigned int round;
    unsigned long source_id;
    int proposal_number;

    message_type type;
    agreement_type agreement;
    in_addr_t ip;
    unsigned short port;
    unsigned int content_size;
    int content[1];
} Message;

Message *create_message(unsigned long source_id, unsigned long sequence_number, void *content, message_type type,
                        in_addr_t ip, unsigned short port, int proposal_number, agreement_type agreement,
                        unsigned int size, unsigned int round);

void to_string(Message *message);

std::string ipReadable(in_addr_t ip);
unsigned short portReadable(unsigned short port);
