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
    }

    // this->sender_logs = this->sender_to_logs_open();
    // this->receiver_logs = this->receiver_to_logs_open();
}

// Creating two threads per socket, one for sending and one for receiving messages.
void UDPSocket::create() {
    std::thread receive_thread(&UDPSocket::receive_message_upgrade, this);
    std::thread send_thread(&UDPSocket::send_message_upgrade, this);
    
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
    this->message_queue_upgrade = other.message_queue_upgrade;
    this->pending_2 = other.pending_2;
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

void UDPSocket::enque_upgrade(unsigned int msg) {

    message_queue_lock.lock();

    for (auto& [id, host] : this->destiantions) {
        if (id != this->localhost.id) { // so we don't keep a queue for myself (as a process)
            message_queue_upgrade[host.id].insert({});
        }
    }

    std::array<unsigned int, 8> payload;

    for (unsigned int i = 1; i<=msg; i++) {
        payload[(i-1)%8] = i;
        std::string msg_prep = "b " + std::to_string(i);
        logs_lock.lock();
        auto it = logs_set.find(msg_prep);
        if (it == logs_set.end()) {
            logs_set.insert(msg_prep);
        }
        logs_lock.unlock();

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
                value.insert(msg_convoy);
                msg_convoy.msg_convoy_print();
                
            }
            this->msg_id++;
            payload.fill(0);
        }
    }
    message_queue_lock.unlock();
}


// UNDER CONSTRUCTION

void UDPSocket::send_message_upgrade() {
    bool infinite_loop = true;
    while (infinite_loop) {

        for (const auto& [key, value] : message_queue_upgrade) {
            if (value.size() > 0) {
                message_queue_lock.lock();
                std::set<Msg_Convoy> copied_message_queue = value;
                message_queue_lock.unlock();

                // std::cout << "\n" << std::endl;
                // std::cout << "Sending for: " << key << std::endl;
                // std::cout << " the message queue size : " << copied_message_queue.size() << std::endl;

                for (Msg_Convoy message : copied_message_queue) {
                    struct sockaddr_in destaddr = this->set_up_destination_address(message.receiver);
                    sendto(this->sockfd, &message, sizeof(message), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));

                    // std::cout << "OG sender: " << message.original_sender << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                }
                std::cout << "\n" << std::endl;
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

        std::string message_group_identifier = std::to_string(message_convoy.original_sender) + "_" + std::to_string(message_convoy.msg_id);
        if (this->drop_message_2.find(message_group_identifier) == this->drop_message_2.end()) {
            std::cout << message_group_identifier << std::endl;
            if (message_convoy.is_ack) {
                Msg_Convoy copied_message_convoy = message_convoy;
                message_queue_lock.lock();
                Parser::Host temp_addr = message_convoy.receiver;
                copied_message_convoy.receiver = copied_message_convoy.sender;
                copied_message_convoy.sender = temp_addr;
                message_queue_upgrade[copied_message_convoy.receiver.id].erase(copied_message_convoy);
                message_queue_lock.unlock();
            }

            else {
                auto it = pending_2.find(message_group_identifier);
                if (it == pending_2.end()) {
                    // The key is not present, insert a new entry with an empty set
                    pending_2[message_group_identifier] = std::set<long unsigned int>();
                }
                // Now you can safely insert into the set associated with the key
                pending_2[message_group_identifier].insert(message_convoy.sender.id);

                // 2. Broadcast further
                Msg_Convoy copied_message_convoy = message_convoy;
                for (const auto& [id, dest] : this->destiantions) {
                    if (id != copied_message_convoy.original_sender && 
                        id != copied_message_convoy.sender.id && 
                        id != this->localhost.id) 
                        {
                        copied_message_convoy.sender = this->localhost;
                        copied_message_convoy.receiver = dest;
                        copied_message_convoy.is_relay = true;

                        message_queue_lock.lock();
                        message_queue_upgrade[id].insert(copied_message_convoy);
                        message_queue_lock.unlock();
                    }
                }

                // 3. Check if enough processes
                if (pending_2[message_group_identifier].size() > this->destiantions.size()/2 &&
                    this->delivered_messages.find(message_group_identifier) == delivered_messages.end()) {
                        std::cout << "message group identifier: " << message_group_identifier << std::endl;
                    deliver_to_logs(message_convoy);
                }
            }

            drop_message_2.insert(message_group_identifier);
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


// ------



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

std::string UDPSocket::get_logs() {
    std::string res;
    for (auto elem : this->logs_set) {
        res = res + elem + "\n";
    }
    std::cout << res << std::endl;
    return res;
}

void UDPSocket::deliver_to_logs(Msg_Convoy message_convoy) {
    // if the message has not yet been delivered
    std::string group_message_identifier = std::to_string(message_convoy.original_sender) + "_" + std::to_string(message_convoy.msg_id);
    if (this->delivered_messages.find(group_message_identifier) == delivered_messages.end()) {
        // write it to the logs file
        for (unsigned int i = 0; i < message_convoy.payload.size(); i++) {
            if (message_convoy.payload[i] != 0) {
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
        delivered_messages.insert(group_message_identifier);
    }
}


std::ofstream UDPSocket::sender_to_logs_open() {
    std::ofstream outFile;
    outFile.open("sender.output", std::ios::trunc);
    return outFile;
}

void UDPSocket::sender_to_logs_write(std::ofstream& outFile, Msg_Convoy message_convoy) {
    if (outFile.is_open()) {
        outFile << "Hello, World!" << std::endl;
    } else {
        std::cerr << "Error writing to file: File is not open." << std::endl;
    }
}

void UDPSocket::sender_to_logs_close(std::ofstream& outFile) {
    outFile.close();
    if (outFile.fail()) {
        std::cerr << "Error closing file: File could not be closed." << std::endl;
    }
}

std::ofstream UDPSocket::receiver_to_logs_open() {
    std::ofstream outFile;
    outFile.open("receiver.output", std::ios::trunc);
    return outFile;
}

void UDPSocket::receiver_to_logs_write(std::ofstream& outFile, Msg_Convoy message_convoy) {
    if (outFile.is_open()) {
        outFile << "Hello, World!" << std::endl;
    } else {
        std::cerr << "Error writing to file: File is not open." << std::endl;
    }
}

void UDPSocket::receiver_to_logs_close(std::ofstream& outFile) {
    outFile.close();
    if (outFile.fail()) {
        std::cerr << "Error closing file: File could not be closed." << std::endl;
    }
}
