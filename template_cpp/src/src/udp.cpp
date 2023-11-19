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
    this->outputFile.open(parser.outputPath(), std::ofstream::out);;
}

// Creating two threads per socket, one for sending and one for receiving messages.
void UDPSocket::create() {
    std::thread receive_thread(&UDPSocket::receive_message_2, this);
    std::thread send_thread(&UDPSocket::send_message_2, this);
    

    /*
    sending 'this' pointer to both thread constructors will allow both constructors to
    operate on the same instance of UDPSocket object    
    */
    send_thread.detach(); 
    receive_thread.detach(); 
}

// Setting private parameters of the UDPSocket class.
UDPSocket& UDPSocket::operator=(const UDPSocket & other) {
    this->logs = other.logs;
    this->localhost = other.localhost;
    this->sockfd = other.sockfd;
    this->msg_id_2 = other.msg_id_2;
    this->message_queue_2 = other.message_queue_2;
    this->received_messages_sender_set = other.received_messages_sender_set;
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
    struct sockaddr_in destaddr = this->set_up_destination_address(dest);
    destination = dest;
    message_queue_2_lock.lock();
    message_queue_2.push_back(msg);
    std::ostringstream oss;
    oss << "b " << msg;
    logs.push_back(oss.str());
    message_queue_2_lock.unlock();
}

void UDPSocket::send_message_2() {
    bool infinite_loop = true;
    while (infinite_loop) {
        message_queue_2_lock.lock();
        std::vector<unsigned int> copied_message_queue = message_queue_2;
        message_queue_2_lock.unlock();

        // in my message I can at most send 8 integers as part of the payload
        // need to have a method to obtain the first 8 messages, of course, if they exist.
        std::array<unsigned int, 8> payload;
        auto curr_queue_size = message_queue_2.size();
        
        for (unsigned int i = 0; i<curr_queue_size; i++) {
            payload[i] = message_queue_2[i];
        }

        struct Msg_Convoy msg_convoy = {
            this->localhost,
            this->destination,
            msg_id_2,
            payload,
            false
        };
        msg_id_2++;

        // send message convoy
        struct sockaddr_in destaddr = this->set_up_destination_address(this->destination);
        sendto(this->sockfd, &msg_convoy, sizeof(msg_convoy), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
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
                    auto it = received_messages_sender_set.find(std::make_tuple(message_convoy.sender.id, message_convoy.payload[i]));

                    if (it != received_messages_sender_set.end()) {
                        
                    } else {
                        std::ostringstream oss;
                        oss << "d " << message_convoy.sender.id << " " << message_convoy.payload[i];
                        logs.push_back(oss.str());
                        std::cout << "Received " << message_convoy.payload[i] << " from " << message_convoy.sender.id << '\n';


                        // if (logs.size() > 5) {
                        //     for (auto const &output: logs) {
                        //         this->outputFile << output << std::endl;
                        //     }
                        //     std::cout << this->outputFile;
                        //     logs.clear();
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

std::vector<std::string> UDPSocket::get_logs() {

    return this->logs;
}