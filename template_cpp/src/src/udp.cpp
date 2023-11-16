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



UDPSocket::UDPSocket(Parser::Host localhost) {
    this->localhost = localhost;
    sockfd = this->setup_socket(localhost);
    msg_id = 0;
}

// Creating two threads per socket, one for sending and one for receiving messages.
void UDPSocket::create() {
    std::thread send_thread(&UDPSocket::send_message, this);
    std::thread receive_thread(&UDPSocket::receive_message, this);

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
    this->msg_id = other.msg_id;
    this->message_queue = other.message_queue;
    this->received_messages = other.received_messages;
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
    struct sockaddr_in destaddr = this->set_up_destination_address(dest);
    struct Msg wrapedMsg = {
        this->localhost,
        dest,
        msg_id,
        msg,
        false
        };
    msg_id++;
    message_queue_lock.lock();
    message_queue.push_back(wrapedMsg);
    std::ostringstream oss;
    oss << "b " << msg;
    logs.push_back(oss.str());
    message_queue_lock.unlock();
}


void UDPSocket::send_message() {
    // Reference: https://stackoverflow.com/questions/5249418/warning-use-of-old-style-cast-in-g just try all of them until no error
    bool infinite_loop = true;
    while(infinite_loop) {
        message_queue_lock.lock();
        std::vector<Msg> copiedMsgQueue = message_queue;
        message_queue_lock.unlock();
        for (auto & wrapedMsg : copiedMsgQueue) {
            struct sockaddr_in destaddr = this->set_up_destination_address(wrapedMsg.receiver);
            sendto(this->sockfd, &wrapedMsg, sizeof(wrapedMsg), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
        }
    }
}

// receive() implements reception of both, normal message as well as an acknowledgement!

void UDPSocket::receive_message() {
    // Reference: https://stackoverflow.com/questions/18670807/sending-and-receiving-stdstring-over-socket
    struct Msg wrapped_message; 
    while (true) {
        // std::this_thread::sleep_for (std::chrono::seconds(1));

        if (recv(this->sockfd, &wrapped_message, sizeof(wrapped_message), 0) < 0) {
            throw std::runtime_error("Receive failed");
        } 

        else {
            if (wrapped_message.is_ack) {
                message_queue_lock.lock();
                message_queue.erase(std::remove(message_queue.begin(), message_queue.end(), wrapped_message), message_queue.end());
                message_queue_lock.unlock();
            } else {
                //normal msg
                if (std::find(received_messages.begin(), received_messages.end(), wrapped_message) != received_messages.end()) {
                    // if already receive
                    // std::cout<< "Rejected " << wrapped_message.content << " from "<< wrapped_message.sender.id << "\n";
                } else {
                    //otherwise, save it
                    received_messages.push_back(wrapped_message);
                    std::ostringstream oss;
                    oss << "d " << wrapped_message.sender.id << " " << wrapped_message.content;
                    logs.push_back(oss.str());
                    // std::cout<< "Received " << wrapped_message.content << " from "<< wrapped_message.sender.id << "\n";
                }    
                // send Ack back to sender
                wrapped_message.is_ack = true;
                struct sockaddr_in destaddr = this->set_up_destination_address(wrapped_message.sender);
                Parser::Host tempAddr = wrapped_message.sender;
                wrapped_message.sender = this->localhost;
                wrapped_message.receiver = tempAddr;

                sendto(this->sockfd, &wrapped_message, sizeof(wrapped_message), 0, reinterpret_cast<const sockaddr *>(&destaddr), sizeof(destaddr));
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