#include<thread>
#include <chrono>         // std::chrono::seconds
#include <algorithm>

#include "udp.hpp"

extern std::ofstream outputFile;
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
    this->logs_set = other.logs_set;
    this->message_queue = other.message_queue;
    this->pending = other.pending;
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

void UDPSocket::enque(unsigned int msg) {

    this->message_queue_lock.lock();

    for (auto& [id, host] : this->destiantions) {
        this->message_queue[host.id].insert({});
    }

    std::array<unsigned int, 8> payload;

    for (unsigned int i = 1; i<=msg; i++) {
        payload[(i-1)%8] = i;
        std::string msg_prep = "b " + std::to_string(i);
        this->logs_lock.lock();
        this->logs_set.insert(msg_prep);
        this->logs_lock.unlock();

        if ( (i % 8 == 0 && i != 0) || (i == msg) ) { // need to create a struct and enque it!
            // 1. add to set for every process
            for (auto& [key, value] : message_queue) {
                // 2. create the Msg struct
                struct Msg_Convoy msg_convoy = {
                    this->localhost,
                    this->localhost.id,
                    this->destiantions[key],
                    this->msg_id,
                    payload,
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

    std::cout << "Enquing .." << std::endl;
    for (const auto& [id, host] : this->destiantions) {
        std::cout << "Host: " << id << std::endl;
        std::cout << "Length of the message queue: " << this->message_queue[id].size() << std::endl;
    }
}




void UDPSocket::send_message() {
    bool infinite_loop = true;
    while (infinite_loop) {

        for (const auto& [host_id, queued_messages] : this->message_queue) {
            if (queued_messages.size() > 0) {
                message_queue_lock.lock();
                std::set<Msg_Convoy> copied_message_queue = queued_messages;
                message_queue_lock.unlock();

                for (const Msg_Convoy& message : copied_message_queue) {
                    struct sockaddr_in destaddr = this->set_up_destination_address(message.receiver);
                    sendto(this->sockfd, &message, sizeof(message), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
    }
}

void UDPSocket::receive_message() {
    struct Msg_Convoy message_convoy;
    while (true) {

        if (recv(this->sockfd, &message_convoy, sizeof(message_convoy), 0) < 0) {
            throw std::runtime_error("Receive failed");
        }

        std::string message_group_identifier = std::to_string(message_convoy.original_sender) + "_" + std::to_string(message_convoy.msg_id);

        if (message_convoy.is_ack) {
            Msg_Convoy copied_message_convoy = message_convoy;

            // std::cout << "Before receiving ack, queue length: " << message_queue[copied_message_convoy.receiver.id].size() << std::endl;
            message_queue_lock.lock();
            Parser::Host temp_addr = message_convoy.receiver;
            copied_message_convoy.receiver = copied_message_convoy.sender;
            copied_message_convoy.sender = temp_addr;
            message_queue[copied_message_convoy.receiver.id].erase(copied_message_convoy);
            message_queue_lock.unlock();

            // std::cout << "After receiving ack, queue length: " << message_queue[copied_message_convoy.receiver.id].size() << std::endl;
        }

        else {
            // std::cout << "Received message" << std::endl;
            if ((this->delivered_messages.find(message_group_identifier) == this->delivered_messages.end())) {
                std::cout << "Message" << std::endl;
                std::cout << message_group_identifier << std::endl;
                std::cout << message_convoy.sender.id << std::endl;

                auto it = pending.find(message_group_identifier);
                if (it == pending.end()) {
                    // The key is not present, insert a new entry with an empty set
                    pending[message_group_identifier] = std::set<long unsigned int>();
                }
                // Now you can safely insert into the set associated with the key
                pending[message_group_identifier].insert(message_convoy.sender.id);

                // 3. Check if enough processes
                if (pending[message_group_identifier].size() > this->destiantions.size()/2 ) 
                {
                    std::cout << "delivering ... " << std::endl;
                    std::string seen_by_processes;
                    for (const auto& process : pending[message_group_identifier]) {
                        seen_by_processes = seen_by_processes + std::to_string(process);
                    }
                    std::cout << seen_by_processes << std::endl;
                    deliver_to_logs(message_convoy);
                }

                // 2. Broadcast further
                Msg_Convoy copied_message_convoy = message_convoy;
                copied_message_convoy.sender = this->localhost;
                for (const auto& [host_id, host_parser] : this->destiantions) {
                    if (host_id != this->localhost.id) {
                        copied_message_convoy.receiver = host_parser;

                    // std::cout << "Message ID: " << message_group_identifier << std::endl;
                    // std::cout << "Size of the queue before: " << this->message_queue[host_id].size() << std::endl;

                    this->message_queue_lock.lock();
                    this->message_queue[host_id].insert(copied_message_convoy);
                    this->message_queue_lock.unlock();

                    // std::cout << "Size of the queue after: " << this->message_queue[host_id].size() << std::endl;
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

std::ostringstream UDPSocket::get_logs() {
    std::string res;
    std::ostringstream oss;

    for (auto elem : this->logs_set) {
        res = res + elem + "\n";
        oss << elem << "\n";
    }
    std::cout << res.size() << std::endl;
    return oss;
}

std::set<std::string> UDPSocket::get_logs_2() {
    std::set<std::string> logs_set_copy = this->logs_set;
    return logs_set_copy;
}

void UDPSocket::deliver_to_logs(Msg_Convoy message_convoy) {
    // if the message has not yet been delivered
    std::string group_message_identifier = std::to_string(message_convoy.original_sender) + "_" + std::to_string(message_convoy.msg_id);
    // write it to the logs file
    for (unsigned int i = 0; i < message_convoy.payload.size(); i++) {
        if (message_convoy.payload[i] != 0) {
            std::string msg_prep = "d " + std::to_string(message_convoy.original_sender) + " " + std::to_string(message_convoy.payload[i]);
            std::cout << "This is the message: " << msg_prep << std::endl;
            logs_lock.lock();
            logs_set.insert(msg_prep);
            logs_lock.unlock();
        }
    }
    delivered_messages.insert(group_message_identifier);
}
