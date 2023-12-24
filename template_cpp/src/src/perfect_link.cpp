#include "perfect_link.hpp"


#include <functional>
#include <stdexcept>
#include <sstream>

// CONSTRUCTOR
Perfect_Link::Perfect_Link(
                            in_addr_t ip, 
                            unsigned short port, 
                            unsigned long id, 
                            File_Logger *logger, 
                            bool enable_listener
                            ) {
    this->sequence_number = 1;
    this->id = id;
    this->port = port;
    this->file_logger = logger;
    this->ip = ip;
    this->enable_listener = enable_listener;
    this->socket_fd = this->create_socket(ip, port);
    this->start_service();
}

// DECONTRUCTOR
Perfect_Link::~Perfect_Link() {
    close(this->socket_fd);
}

int Perfect_Link::create_socket(in_addr_t ip, unsigned short port) {
    // Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create the socket!");
    }

    // Set up server address structure
    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ip;
    servaddr.sin_port = port;

    // Bind socket to the address
    if (bind(sockfd, reinterpret_cast<const sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to bind the socket!");
    }

    return sockfd;
}

void Perfect_Link::start_service() {
    std::thread cleanup_thread(&Perfect_Link::cleanup, this);
    std::thread retry_thread(&Perfect_Link::retry, this);

    if (this->enable_listener) {
        std::thread listener_thread(&Perfect_Link::listen, this);
        listener_thread.detach();
    }
    cleanup_thread.detach();
    retry_thread.detach();
}










void Perfect_Link::send(in_addr_t ip, unsigned short port, void *content, message_type type, bool logging, 
                       unsigned long source_id, int proposal_number, unsigned int size, 
                       agreement_type agreement,  unsigned int round, unsigned long seq_no) {
    struct sockaddr_in clientaddr;
    int socketfd;
    Message *message;

    if (type == SYN) {
        message = create_message(
                                source_id, 
                                seq_no, 
                                content, 
                                type, 
                                ip, 
                                port, 
                                proposal_number, 
                                agreement, 
                                size, 
                                round);
    } else if (type == ACK) {
        message = create_message(
                                source_id, 
                                seq_no, 
                                content, 
                                type, 
                                this->ip, 
                                this->port, 
                                proposal_number, 
                                agreement, 
                                size, 
                                round);
    } else {
        message = create_message(
                                source_id, 
                                seq_no, 
                                content, 
                                type, 
                                ip, 
                                port, 
                                proposal_number, 
                                agreement, 
                                size, 
                                round);
    }

    if (message == NULL) {
        std::cout << "Cannot create the message\n" << std::flush;
        return;
    }

    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family        = AF_INET;
    clientaddr.sin_addr.s_addr   = ip;
    clientaddr.sin_port          = port;

    long int res = sendto(this->socket_fd, message, 
                offsetof(Message, content) + size, 0,
                reinterpret_cast<struct sockaddr*>(&clientaddr), 
                sizeof(clientaddr));

    if (res == -1) {
        return;
    }

    std::stringstream ack_key;
    ack_key << ipReadable(clientaddr.sin_addr.s_addr) << ":" << portReadable(clientaddr.sin_port);
    ack_key << "_" << message->sequence_number << "_" << message->source_id;

    std::string key = ack_key.str();

    // if (message->type == SYN || message->type == BROADCAST) {
    //     this->add_element_queue.lock();
    //     this->message_queue.insert({key, NOT_RECEIVED});

    //     this->message_history.insert({key, message});
    //     this->add_element_queue.unlock();
    //     if (logging) {
    //         this->file_logger->log_broadcast(this->sequence_number);
    //     }
    // } else if (message->type == RSYN) {
    //     this->add_element_queue.lock();
    //     this->message_queue.insert({ack_key.str(), NOT_RECEIVED});
    //     this->message_history.insert({ack_key.str(), message});
    //     this->add_element_queue.unlock();
    // }

    auto updateQueue = [this](const std::string& ackKey, ack_status status, Message* msg) {
        this->add_element_queue.lock();
        this->message_queue.insert({ackKey, status});
        this->message_history.insert({ackKey, msg});
        this->add_element_queue.unlock();
    };

    if (message->type == SYN || message->type == BROADCAST) {
        updateQueue(key, NOT_RECEIVED, message);

        if (logging) {
            this->file_logger->log_broadcast(this->sequence_number);
        }
    } else if (message->type == RSYN) {
        updateQueue(ack_key.str(), NOT_RECEIVED, message);
    }
}

void Perfect_Link::send_syn() {}

void Perfect_Link::send_rsyn() {}











Message* Perfect_Link::receive(bool logging, unsigned int size) {
    struct sockaddr_in sourceaddr;
    Message *header = reinterpret_cast<Message *>(malloc(offsetof(Message, content)));
    if (header == NULL) return NULL;
    memset(&sourceaddr, 0, sizeof(sourceaddr));

    socklen_t len = sizeof(sourceaddr);
    long int size_packet = recvfrom(
                            this->socket_fd, 
                            header, 
                            offsetof(Message, content), 
                            MSG_PEEK, 
                            reinterpret_cast<struct sockaddr*>(&sourceaddr), 
                            &len
                            );
    
    if (size_packet < 0) {
        return NULL;
    }

    Message *data_recv = NULL;
    if (header->content_size > 0) {
        data_recv = reinterpret_cast<Message *>(malloc(header->content_size + offsetof(Message, content)));
        if (data_recv == NULL) {
            return NULL;
        }

        long int res = recvfrom(this->socket_fd, data_recv, offsetof(Message, content) + header->content_size, 0, 
                                reinterpret_cast<struct sockaddr*>(&sourceaddr), 
                                &len);
        if (res < 0) {
            free(header);
            return NULL;
        }
    } else {
        data_recv = reinterpret_cast<Message *>(malloc(sizeof(Message)));
        if (data_recv == NULL) {
            return NULL;
        }
        long int res = recvfrom(this->socket_fd, data_recv, sizeof(Message), 0, 
                                reinterpret_cast<struct sockaddr*>(&sourceaddr), 
                                &len);
        
    }

    Message *recv_message = create_message(data_recv->source_id, data_recv->sequence_number, data_recv->content,
                                                data_recv->type, data_recv->ip, data_recv->port,
                                                data_recv->proposal_number, data_recv->agreement, 
                                                data_recv->content_size, data_recv->round);

    if (data_recv != NULL) {
        free(data_recv);
        data_recv = NULL;
    }

    if (header != NULL) {
        free(header); 
        header = NULL;
    }

    if (recv_message == NULL) {
        return NULL;
    }

    if (recv_message == NULL || recv_message->source_id == 0) {
        std::cout << "error received message" << std::endl << std::flush;
        return NULL;
    }

    std::stringstream ack_key;

    if (recv_message->type == ACK) {
        ack_key << ipReadable(recv_message->ip) << ":" << portReadable(recv_message->port);
        ack_key << "_" << recv_message->sequence_number << "_" << recv_message->source_id;
        
        this->add_element_queue.lock();
        if (this->message_queue.find(ack_key.str()) != this->message_queue.end()) {
            this->message_queue[ack_key.str()] = DELETED;
        }
        this->add_element_queue.unlock();
    } else {
        ack_key << ipReadable(sourceaddr.sin_addr.s_addr) << ":" << portReadable(sourceaddr.sin_port);
        ack_key << "_" << recv_message->sequence_number;

        this->send(sourceaddr.sin_addr.s_addr, sourceaddr.sin_port, NULL, ACK, false, recv_message->source_id, 
                   0, 0, ACKNOWLEDGEMENT, recv_message->round);
        
            auto found = this->received_message.find(ack_key.str());
            if (found == this->received_message.end()) {
                this->received_message.insert({ack_key.str(), true});
                if (logging) {
                    this->file_logger->log_deliver(recv_message->source_id, recv_message->sequence_number);
                }
            }
    }

    if (recv_message->type == SYN || recv_message->type == RSYN) {
        Message *returned_message = create_message(recv_message->source_id, recv_message->sequence_number, recv_message->content,
                                                recv_message->type, recv_message->ip, recv_message->port,
                                                recv_message->proposal_number, recv_message->agreement, 
                                                recv_message->content_size, recv_message->round);
        if (recv_message != NULL) {
            free(recv_message);
            recv_message = NULL;
        }
        return returned_message;
    }
    
    if (recv_message != NULL) {
        free(recv_message);
        recv_message = NULL;
    }
    return NULL;
}




void Perfect_Link::listen() {
    bool infinity = true;
    while(infinity) {
        this->receive(true, 0);
    }
}

void Perfect_Link::cleanup() {
    bool infinity = true;
    while (infinity) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CLEANUP_TIME_INTERVAL));

        {
            std::unique_lock<std::mutex> lock(this->add_element_queue);

            for (auto iterator = this->message_queue.begin(); iterator != this->message_queue.end();) {
                if (iterator->second == DELETED) {
                    const std::string key = std::move(iterator->first);

                    if (this->message_history[key] != nullptr) {
                        free(this->message_history[key]);
                        this->message_history[key] = nullptr;
                    }

                    this->message_history.erase(key);
                    iterator = this->message_queue.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
    }
}

void Perfect_Link::retry() {
    bool infinity = true;
    while (infinity) {
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_TIME_INTERVAL_PF));

        std::unordered_map<std::string, ack_status> retry_messages;
        {
            std::unique_lock<std::mutex> lock(this->add_element_queue);
            retry_messages = this->message_queue;
        }

        for (auto& [key, status] : retry_messages) {
            if (status == NOT_RECEIVED || status == NOT_SEND) {
                Message* aux_message = nullptr;

                {
                    std::unique_lock<std::mutex> lock(this->add_element_queue);
                    aux_message = this->message_history[key];
                }

                if (aux_message == nullptr) {
                    continue;
                }

                Message* message = create_message(
                    aux_message->source_id, aux_message->sequence_number, aux_message->content,
                    RSYN, aux_message->ip, aux_message->port,
                    aux_message->proposal_number, aux_message->agreement, aux_message->content_size,
                    aux_message->round
                );

                if (message == nullptr) {
                    std::cout << "Cannot create message.\n" << std::flush;
                    continue;
                }

                this->send(
                    message->ip, message->port, message->content,
                    RSYN, true, message->source_id, message->proposal_number,
                    message->content_size, message->agreement,
                    message->round, message->sequence_number
                );

                if (message != nullptr) {
                    free(message);
                }
            }
        }
    }
}


// getters
unsigned long Perfect_Link::get_id() {
    return this->id;
}
unsigned long Perfect_Link::get_sequence_number() {
    return this->sequence_number;
}
int Perfect_Link::get_socket_fd() {
    return this->socket_fd;
}
std::unordered_map<std::string, Message *> Perfect_Link::get_message_history() {
    return this->message_history;
}
std::unordered_map<std::string, ack_status> Perfect_Link::get_message_queue() {
    return this->message_queue;
}