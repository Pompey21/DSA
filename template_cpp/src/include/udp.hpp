#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include "parser.hpp"
#include "message_old.hpp"
#include <set>
#include <unordered_set>
#include <tuple>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>

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
        UDPSocket(Parser::Host localhost, Parser parser); // constructor that takes a Parser::Host argument

        void create();
        void enque(unsigned int msg);

        std::vector<std::string> get_logs_3();

        UDPSocket& operator=(const UDPSocket & other);

    private:
    // assignable:
        Parser::Host localhost;
        std::unordered_map<unsigned long, Parser::Host> destiantions;
        int sockfd; // socket file descriptor
        unsigned long msg_id;
        std::mutex logs_lock;
        std::vector<std::string> logs_vector;
        
        std::mutex message_queue_lock;
        // process -> his queue of messages // map_of_message_queues
        std::unordered_map<unsigned long, std::set<Msg_Convoy>> map_of_message_queues;

        // original sender + message id -> set([processes that have seen it])
        std::map<std::string, std::set<unsigned long>> pending;
        std::set<std::string> delivered_messages;
        std::map<std::string, Msg_Convoy> messages_prepared_for_delivery;

        // infrastructure
        int setup_socket(Parser::Host host);
        struct sockaddr_in set_up_destination_address(Parser::Host dest);

        // modified methods
        void send_message();
        void receive_message();
        void deliver_to_logs(Msg_Convoy msg_convoy);
        std::tuple<std::string, std::string> split_message_identifier(std::string message_group_identifier);     
};