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
    msg_id_2 = 0;

    // need to initiate destinations_2 & destinations
    for (auto host : parser.hosts()) {
        Parser::Host host_og = host;
        this->destiantions_2[host.id] = host;
        std::cout << "This is the host ID: " << destiantions_2[host.id].id << std::endl;
        std::cout << "This is the host IP: " << destiantions_2[host.id].ip << std::endl;
    }
    std::cout << "\nSize of my destinations_2: " << destiantions_2.size() << std::endl;
}

// Creating two threads per socket, one for sending and one for receiving messages.
void UDPSocket::create() {
    std::thread receive_thread(&UDPSocket::receive_message_2, this);
    // std::thread send_thread(&UDPSocket::send_message_2, this);
    // std::thread send_thread(&UDPSocket::send_message, this);
    std::thread send_thread(&UDPSocket::send_message_deluxe, this);
    
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
    this->sockfd = other.sockfd;
    this->msg_id_2 = other.msg_id_2;
    this->message_queue_2 = other.message_queue_2;
    this->received_messages_sender_set = other.received_messages_sender_set;
    this->logs_set = other.logs_set;
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

void UDPSocket::enque_2(Parser::Host dest, unsigned int msg) {

    this->destination = dest;

    message_queue_2_lock.lock();
    message_queue_2.push_back(msg);

    std::string msg_prep = "b " + std::to_string(msg);
    // std::cout << "This is the message prep: " << msg_prep << std::endl;
    logs_lock.lock();
    auto it = logs_set.find(msg_prep);
    if (it == logs_set.end()) {
        logs_set.insert(msg_prep);
    }
    logs_lock.unlock();
    
    message_queue_2_lock.unlock();
}

void UDPSocket::enque(Parser::Host dest, unsigned int msg) {

    // std::cout << "Size of my destinations_2: " << this->destiantions_2.size() << std::endl;

    message_queue_2_lock.lock();
    // check if there has already been an array inserted
    if (message_queue.find(dest.id) != message_queue.end()) {
        // YES -> append msg
        message_queue[dest.id].push_back(msg);
    } else {
        // NO -> insert vector with msg 
        message_queue[dest.id].assign({msg});
    }

    if (message_queue_deluxe.find(dest.id) != message_queue_deluxe.end()) {
        // YES -> add msg
        message_queue_deluxe[dest.id].insert(msg);
    } else {
        // NO -> insert new set with msg
        message_queue_deluxe[dest.id].insert({msg});
    }

    message_queue_2_lock.unlock();

    std::string msg_prep = "b " + std::to_string(msg);
    // std::cout << "This is the message prep: " << msg_prep << std::endl;
    logs_lock.lock();
    auto it = logs_set.find(msg_prep);
    if (it == logs_set.end()) {
        logs_set.insert(msg_prep);
    }
    logs_lock.unlock();
}

void UDPSocket::send_message_deluxe() {
    bool infinite_loop = true;
    while (infinite_loop) {

        for (const auto& [key, value] : message_queue_deluxe) {
            std::cout << "Key: " << key << std::endl;

            if (value.size() > 0) {
                message_queue_2_lock.lock();
                std::set<unsigned int> copied_message_queue = value;
                message_queue_2_lock.unlock();
                
                std::cout << "This is the message queue size : " << value.size() << std::endl;

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
                    // this->destination, // this I now need to change
                    this->destiantions_2[key],
                    this->msg_id_2,
                    payload,
                    false
                };
                msg_id_2++;

                // send message convoy
                // struct sockaddr_in destaddr = this->set_up_destination_address(this->destiantions_2[key]);
                struct sockaddr_in destaddr = this->set_up_destination_address(msg_convoy.receiver);
                sendto(this->sockfd, &msg_convoy, sizeof(msg_convoy), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));

                std::cout << "Sending message ... " << std::endl;
                // std::cout << "Destination address: " << destaddr.sin_addr.s_addr << std::endl;
                std::cout << "Destination ID: " << this->destiantions_2[key].id << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(4));
            }
        }
    }
}


// receive() implements reception of both, normal message as well as an acknowledgement!
void UDPSocket::receive_message_2() {
    struct Msg_Convoy message_convoy;
    while (true) {
        if (recv(this->sockfd, &message_convoy, sizeof(message_convoy), 0) < 0) {
            throw std::runtime_error("Receive failed");
        } 

        else {

            // std::cout << "At least we receive something!" << std::endl;
            
            if (message_convoy.is_ack) {
                // need to parse the message
                // erase them from my message queue
                message_queue_2_lock.lock();
                std::array<unsigned int, 8> payload = message_convoy.payload;

                // remove every message from the queue for which I received the ack
                for(unsigned int i = 0; i < payload.size(); i++)
                {
                    message_queue_2.erase(std::remove(message_queue_2.begin(), message_queue_2.end(), payload[i]), message_queue_2.end());
                }
                message_queue_2_lock.unlock();

            }

            else {
                // if we haven't received it yet, then we need to save it

                for (unsigned int i = 0; i < message_convoy.payload.size(); i++) {
                    // std::cout << message_convoy.payload[i] << std::endl;
                    auto it = received_messages_sender_set.find(std::make_tuple(message_convoy.sender.id, message_convoy.payload[i]));
                    if (it != received_messages_sender_set.end() || message_convoy.payload[i] == 0) {
                        
                    } else {
                        std::ostringstream oss;
                        oss << "d " << message_convoy.sender.id << " " << message_convoy.payload[i];

                        logs_lock.lock();
                        std::string msg_prep = "d " + std::to_string(message_convoy.sender.id) + " " + std::to_string(message_convoy.payload[i]);
                        std::cout << "This is the message: " << msg_prep << std::endl;
                        auto it = logs_set.find(msg_prep);
                        if (it == logs_set.end()) {
                            logs_set.insert(msg_prep);
                        }
                        // logs_set.insert(msg_prep);
                        logs_lock.unlock();

                        // if (logs.size() > 5) {
                        //     for (auto const &output: logs) {
                        //         // this->outputFile << output << std::endl;
                        //         std::cout << "Hllo Rares: " << output << std::endl;
                        //     }
                        //     // std::cout << this->outputFile;
                            
                        //     // this->outputFile.flush();
                        //     // logs.clear();
                        // }

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

std::vector<std::string> UDPSocket::get_logs_2() {
    std::vector<std::string> res;
    for (auto elem : this->logs_set) {
        res.push_back(elem);
    }

    return res;
}
