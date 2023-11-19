#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include "parser.hpp"
#include "message.hpp"
#include "message_2.hpp"
#include <set>
#include <tuple>

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
        // constructors
        UDPSocket(){}; // defautl constructor
        UDPSocket(const UDPSocket &); // copy constructor
        UDPSocket(Parser::Host localhost); // constructor that takes a Parser::Host argument

        void create();
        void enque(Parser::Host dest, unsigned int msg);
        void enque_2(Parser::Host dest, unsigned int msg);
        std::vector<std::string> get_logs();
        UDPSocket& operator=(const UDPSocket & other);

    private:
    // assignable:
        Parser::Host localhost;
        Parser::Host destination;
        int sockfd; // socket file descriptor
        unsigned long msg_id;
        unsigned long msg_id_2;

        std::vector<std::string> logs;
        std::vector<Msg> message_queue;
        std::vector<unsigned int> message_queue_2;
        std::mutex message_queue_2_lock;
        std::mutex message_queue_lock;

        std::vector<Msg> received_messages;

        std::set<unsigned int> received_messages_set;
        std::set<std::tuple<unsigned int, unsigned int>> received_messages_sender_set;

        int setup_socket(Parser::Host host);
        struct sockaddr_in set_up_destination_address(Parser::Host dest);

        void send_message();
        void send_message_2();
        void receive_message();
        void receive_message_2();

        std::vector<unsigned int> message_convoy_parser(Msg_Convoy);
};