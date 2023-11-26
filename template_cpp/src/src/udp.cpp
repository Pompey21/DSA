#include<thread>
#include <chrono>         // std::chrono::seconds
#include <algorithm>

#include "udp.hpp"

/*
// References and Resources: 
https://www.geeksforgeeks.org/udp-server-client-implementation-c/
*/

/*
Basic Idea Description

The entire process of UDP Server-Client Implementation can be broken down into 
smaller steps for each of the two processes. The steps are as follows:

UDP Server :  
Create a UDP socket.
Bind the socket to the server address.
Wait until the datagram packet arrives from the client.
Process the datagram packet and send a reply to the client.
Go back to Step 3.

UDP Client :  
Create a UDP socket.
Send a message to the server.
Wait until response from the server is received.
Process reply and go back to step 2, if necessary.
Close socket descriptor and exit.
*/

UDPSocket::UDPSocket(Parser::Host localhost, Parser parser) {
    this->localhost = localhost;
    sockfd = this->setup_socket(localhost);
    msg_id = 0;

    for (auto host : parser.hosts()) {
        Parser::Host host_og = host;
        this->destiantions[host.id] = host;
        std::cout << "This is the host ID: " << destiantions[host.id].id << std::endl;
        std::cout << "This is the host IP: " << destiantions[host.id].ip << std::endl;
    }
}

// Creating two threads per socket, one for sending and one for receiving messages.
void UDPSocket::create() {
    std::thread receive_thread(&UDPSocket::receive_message, this);
    std::thread send_thread(&UDPSocket::send_message, this);
    
    /*
    sending 'this' pointer to both thread constructors will allow both constructors to
    operate on the same instance of UDPSocket object    
    */
    send_thread.detach(); 
    receive_thread.detach(); 
}

// Setting private parameters of the UDPSocket class.
UDPSocket& UDPSocket::operator=(const UDPSocket & other) {
    this->localhost = other.localhost;
    this->destiantions = other.destiantions;
    this->sockfd = other.sockfd;
    this->msg_id = other.msg_id;
    this->received_messages_sender_set = other.received_messages_sender_set;
    this->logs_set = other.logs_set;
    this->message_queue = other.message_queue;
    this->message_queue_upgrade = other.message_queue_upgrade;
    this->pending = other.pending;
    this->delivered_messages = other.delivered_messages;
    return *this;
}

struct sockaddr_in UDPSocket::set_up_destination_address(Parser::Host dest) {
    struct sockaddr_in destaddr;
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET; //IPv4
    destaddr.sin_addr.s_addr = dest.ip;
    destaddr.sin_port = dest.port;
    return destaddr;
}


void UDPSocket::enque(Parser::Host dest, unsigned int msg) {
    message_queue_lock.lock();
    // check if there has already been an array inserted
    if (message_queue.find(dest.id) != message_queue.end()) {
        // YES -> add msg
        message_queue[dest.id].insert(msg);
    } else {
        // NO -> insert new set with msg
        message_queue[dest.id].insert({msg});
    }
    message_queue_lock.unlock();

    std::string msg_prep = "b " + std::to_string(msg);
    logs_lock.lock();
    auto it = logs_set.find(msg_prep);
    if (it == logs_set.end()) {
        logs_set.insert(msg_prep);
    }
    logs_lock.unlock();
}

void UDPSocket::enque_upgrade(unsigned int msg) {

    message_queue_lock.lock();

    for (auto& [key, value] : this->destiantions) {
        message_queue_upgrade[value.id].insert({});
    }

    std::array<unsigned int, 8> payload;

    for (unsigned int i = 0; i<=msg; i++) {
        if ( (i % 8 == 0 && i != 0) || (i == msg) ) { // need to create a struct and enque it!
            // 1. add to set for every process
            for (auto& [key, value] : message_queue_upgrade) {

                // 2. create the Msg struct
                struct Msg_Convoy msg_convoy = {
                    this->localhost,
                    this->localhost.id,
                    this->destiantions[key],
                    this->msg_id,
                    payload,
                    false,
                    false
                };

                if (key != this->localhost.id) {
                    value.insert(msg_convoy);
                    // msg_convoy.msg_convoy_print();
                }
            }
            payload.fill(0);
        }
        payload[i%8] = i;
    }
    message_queue_lock.unlock();
}


// UNDER CONSTRUCTION

void UDPSocket::send_message_upgrade() {
    bool infinite_loop = true;
    while (infinite_loop) {

        for (const auto& [key, value] : message_queue_upgrade) {
            std::cout << "Key: " << key << std::endl;

            if (value.size() > 0) {
                message_queue_lock.lock();
                std::set<Msg_Convoy> copied_message_queue = value;
                message_queue_lock.unlock();

                std::cout << "This is the message queue size : " << copied_message_queue.size() << std::endl;

                for (const auto& message : copied_message_queue) {
                    std::cout << "Sending the message" << std::endl;

                    struct sockaddr_in destaddr = this->set_up_destination_address(message.receiver);
                    sendto(this->sockfd, &message, sizeof(message), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));

                    std::cout << "Sending message ... " << std::endl;
                    std::cout << "Destination ID: " << message.receiver.id << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                }
            }
        }
    }
}

void UDPSocket::receive_message_upgrade() {
    struct Msg_Convoy message_convoy;
    while (true) {

        if (recv(this->sockfd, &message_convoy, sizeof(message_convoy), 0) < 0) {
            throw std::runtime_error("Receive failed");
        }

        else {
            if (message_convoy.is_ack) {
                // need to parse the message => remove from my queues
                if (message_convoy.is_relay) {
                    // if not yet received from this process, add it to Pending Counter
                    if (!(pending[message_convoy.original_sender][message_convoy].find(message_convoy.original_sender) != pending[message_convoy.original_sender][message_convoy].end())) {
                        pending[message_convoy.original_sender][message_convoy].insert(message_convoy.original_sender);
                    }

                    // check if I have enough ACKS => deliver
                    if (pending[message_convoy.original_sender][message_convoy].size() >= this->destiantions.size()/2) {
                        if (!(delivered_messages.find(message_convoy) != delivered_messages.end())) {
                            delivered_messages.insert(message_convoy);

                            // write it to the logs file
                            for (unsigned int i = 0; i < message_convoy.payload.size(); i++) {
                                std::ostringstream oss;
                                oss << "d " << message_convoy.original_sender << " " << message_convoy.payload[i];

                                logs_lock.lock();
                                std::string msg_prep = "d " + std::to_string(message_convoy.original_sender) + " " + std::to_string(message_convoy.payload[i]);
                                std::cout << "This is the message: " << msg_prep << std::endl;
                                auto it = logs_set.find(msg_prep);
                                if (it == logs_set.end()) {
                                    logs_set.insert(msg_prep);
                                }
                                logs_lock.unlock();
                            }
                        }
                    }
                } 
                message_queue_lock.lock();
                // remove every message from the queue for which I received the ack => queue of that given process
                message_queue_upgrade[message_convoy.sender.id].erase(message_convoy);
                message_queue_lock.unlock();
            }

            else {
                // if we have not received it yet, then we need to save it
                if (!(pending[message_convoy.original_sender].find(message_convoy) != pending[message_convoy.original_sender].end())) {
                    // 1. add it to the Pending variable
                    pending[message_convoy.original_sender][message_convoy].insert({message_convoy.original_sender});

                    // 2. enque it to be sent to all other processes
                    for (const auto& [id, dest] : this->destiantions) {
                        if (id != message_convoy.original_sender && id != message_convoy.sender.id && id != this->localhost.id) {
                            message_convoy.sender = this->localhost;
                            message_convoy.receiver = dest;
                            message_convoy.is_relay = true;

                            message_queue_upgrade[id].insert(message_convoy);
                        }
                    }
                }
                
                // send the Ack back to sender
                message_convoy.is_ack = true;
                struct sockaddr_in destaddr = this->set_up_destination_address(message_convoy.sender);
                Parser::Host tempAddr = message_convoy.sender;
                message_convoy.sender = this->localhost;
                message_convoy.receiver = tempAddr;

                sendto(this->sockfd, &message_convoy, sizeof(message_convoy), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
            }
        }
    }    
}


// ------


void UDPSocket::send_message() {
    bool infinite_loop = true;
    while (infinite_loop) {

        for (const auto& [key, value] : message_queue) {
            std::cout << "Key: " << key << std::endl;

            if (value.size() > 0) {
                message_queue_lock.lock();
                std::set<unsigned int> copied_message_queue = value;
                message_queue_lock.unlock();
                
                std::cout << "This is the message queue size : " << copied_message_queue.size() << std::endl;

                // iteration maximum
                unsigned long iteration_maximum;
                if (value.size() > 8) {
                    iteration_maximum = 8;
                } else {
                    iteration_maximum = value.size();
                }

                // in my message I can at most send 8 integers as part of the payload
                // need to have a method to obtain the first 8 messages, of course, if they exist.
                std::array<unsigned int, 8> payload;

                unsigned long i = 0;
                for (auto msg : copied_message_queue) {
                    if (i < iteration_maximum) {
                        payload[i] = msg;
                        i++;
                    } else {
                        break;
                    }
                }

                struct Msg_Convoy msg_convoy = {
                    this->localhost,
                    this->localhost.id,
                    this->destiantions[key],
                    this->msg_id,
                    payload,
                    false,
                    false
                };

                // send message convoy
                struct sockaddr_in destaddr = this->set_up_destination_address(msg_convoy.receiver);
                sendto(this->sockfd, &msg_convoy, sizeof(msg_convoy), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));

                std::cout << "Sending message ... " << std::endl;
                std::cout << "Destination ID: " << this->destiantions[key].id << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(4));
            }
        }
        this->msg_id++;
    }
}


// receive() implements reception of both, normal message as well as an acknowledgement!
void UDPSocket::receive_message() {
    struct Msg_Convoy message_convoy;
    while (true) {

        if (recv(this->sockfd, &message_convoy, sizeof(message_convoy), 0) < 0) {
            throw std::runtime_error("Receive failed");
        }

        else {
            if (message_convoy.is_ack) {
                // need to parse the message => remove from my queues
                message_queue_lock.lock();

                std::array<unsigned int, 8> payload = message_convoy.payload;

                // remove every message from the queue for which I received the ack => queue of that given process
                for (unsigned int i = 0; i < payload.size(); i++) {
                    // is it okey even if the element does not exist in the set????
                    message_queue[message_convoy.sender.id].erase(payload[i]);
                }
                message_queue_lock.unlock();
            }

            else {
                // if we have not received it yet, then we need to save it 

                for (unsigned int i = 0; i < message_convoy.payload.size(); i++) {
                    
                    auto it = received_messages_sender_set.find(std::make_tuple(message_convoy.sender.id, message_convoy.payload[i]));
                    if (!(it != received_messages_sender_set.end() || message_convoy.payload[i] == 0)) {
                        std::ostringstream oss;
                        oss << "d " << message_convoy.sender.id << " " << message_convoy.payload[i];

                        logs_lock.lock();
                        std::string msg_prep = "d " + std::to_string(message_convoy.sender.id) + " " + std::to_string(message_convoy.payload[i]);
                        std::cout << "This is the message: " << msg_prep << std::endl;
                        auto it = logs_set.find(msg_prep);
                        if (it == logs_set.end()) {
                            logs_set.insert(msg_prep);
                        }
                        logs_lock.unlock();

                        received_messages_sender_set.insert(std::make_tuple(message_convoy.sender.id, message_convoy.payload[i]));
                    }
                }

                // send the Ack back to sender
                message_convoy.is_ack = true;
                struct sockaddr_in destaddr = this->set_up_destination_address(message_convoy.sender);
                Parser::Host tempAddr = message_convoy.sender;
                message_convoy.sender = this->localhost;
                message_convoy.receiver = tempAddr;

                sendto(this->sockfd, &message_convoy, sizeof(message_convoy), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
            }
        }
    }
}




int UDPSocket::setup_socket(Parser::Host host) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Socket creation failed");
    }
    struct sockaddr_in hostaddr;
    memset(&hostaddr, 0, sizeof(hostaddr));
    hostaddr.sin_family = AF_INET; //IPv4
    hostaddr.sin_addr.s_addr = host.ip;
    hostaddr.sin_port = host.port;

    if (bind(sockfd, reinterpret_cast<const sockaddr *>(&hostaddr), sizeof(hostaddr)) < 0) {
        throw std::runtime_error("Bind failed");
    }
    return sockfd;
}

std::vector<std::string> UDPSocket:() {
    std::vector<std::string> res;
    for (auto elem : this->logs_set) {
        res.push_back(elem);
    }
    return res;
}
