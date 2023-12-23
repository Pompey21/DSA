#pragma once

#include "message.hpp"
#include "file_logger.hpp"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include <mutex>
#include <ctime>
#include <thread>
#include <string>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

#define WAIT_BEFORE_CLEAN 500
#define WAIT_TO_RETRY 501

class Perfect_Link {
    public:
        std::mutex insert_history;
        std::mutex read_history;
        std::unordered_map<std::string, bool> received_message;
        File_Logger *logger;

        Perfect_Link(in_addr_t ip, unsigned short port, unsigned long id, File_Logger *logger, bool enable_listener);
        ~Perfect_Link();

        void send(in_addr_t ip, unsigned short port, void *content, message_type type, bool logging,
                unsigned long source_id, int proposal_number, unsigned int size, agreement_type agreement,
                unsigned int round, unsigned long seq_no = 0);
        Message* receive(bool logging, unsigned int size);

        unsigned long getSeqNo();
        unsigned long getID();

    private:
        unsigned long id;
        unsigned long seq_no;
        unsigned short port;
        int socket_fd;
        bool enable_listener;
        in_addr_t ip;

        std::mutex write_logs;
        std::mutex add_element_queue;

        std::unordered_map<std::string, Message *> message_history;
        std::unordered_map<std::string, ack_status> message_queue;

        int createSocket(in_addr_t ip, unsigned short port);
        void startService();
        void listen();
        void cleanup();
        void retry();
};