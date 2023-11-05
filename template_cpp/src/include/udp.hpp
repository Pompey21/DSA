#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include "parser.hpp"
#include "message.hpp"

/*
Idea is to create an infrastructure for a basic UDP socket that can send and receive messages.
There are different ways of initiating a socket -> based on input provided.
There are some basic functions that are required for a socket to be able to send and receive messages.
These functions are:
void create() -> starts the socket.
void enque() -> puts a message in the queue to be sent.
std::vector<Msg> get_logs() -> gets all the messages that have been received.
They will all be implemented in the .cpp file, but will be declared here.
*/

class UDPSocket {
    public:
        UDPSocket(){};
        UDPSocket(const UDPSocket &);
        UDPSocket(Parser::Host localhost);

        void create();
        void enque(Parser::Host dest, unsigned int msg);
        std::vector<std::string> get_logs();
        UDPSocket& operator=(const UDPSocket & other);

    private:
    // assignable:
        Parser::Host localhost;
        int sockfd; // socket file descriptor
        unsigned long msg_id;

        std::vector<std::string> logs;
        std::vector<Msg> message_queue;
        std::mutex message_queue_lock;

        std::vector<Msg> received_messages;
        int setup_socket(Parser::Host host);
        struct sockaddr_in set_up_destination_address(Parser::Host dest);

        void send_message();
        void receive_message();
};