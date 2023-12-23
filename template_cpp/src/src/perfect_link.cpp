#include "perfect_link.hpp"

using namespace std;

/*
    TODO  de modificat structura mesajului si practic acel pointer sa fie de o dimensiune fixa vs/ds 
    trebuie verificat din nou enuntul. 
    O problema care poate aparea este urmatoarea: sa am ori NULL la content ori un set. Trebuie regandit un pic
*/

PerfectLink::PerfectLink(in_addr_t ip, unsigned short port, unsigned long id, Logger *logger, bool enable_listener) {
    this->seq_no = 1;
    this->id = id;
    this->port = port;
    this->logger = logger;
    this->ip = ip;
    this->enable_listener = enable_listener;
    this->socket_fd = this->createSocket(ip, port);
    this->startService();
}

PerfectLink::~PerfectLink() {
    close(this->socket_fd);
}

int PerfectLink::createSocket(in_addr_t ip, unsigned short port) {
    struct sockaddr_in servaddr;
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("Failed to create the socket!"); 
        exit(EXIT_FAILURE); 
    }
    
    memset(&servaddr, 0, sizeof(servaddr)); 
     
    servaddr.sin_family    = AF_INET; 
    servaddr.sin_addr.s_addr = ip;
    servaddr.sin_port = port; 
            
    auto addr = reinterpret_cast<const struct sockaddr *>(&servaddr);
            
    if (bind(sockfd, addr, sizeof(servaddr)) < 0) 
    { 
        perror("Failed to bind the socket!"); 
        exit(EXIT_FAILURE); 
    } 

    return sockfd;
}

void PerfectLink::startService() {
    thread cleanup_thread(&PerfectLink::cleanup, this);
    thread retry_thread(&PerfectLink::retry, this);
    // thread delete_backup_data(&PerfectLink::delete_backup_data, this);

    if (this->enable_listener) {
        thread listener_thread(&PerfectLink::listen, this);
        listener_thread.detach();
    }
    cleanup_thread.detach();
    retry_thread.detach();
    // delete_backup_data.detach();
}


void PerfectLink::send(in_addr_t ip, unsigned short port, void *content, message_type type, bool logging, 
                       unsigned long source_id, int proposal_number, unsigned int size, 
                       agreement_type agreement,  unsigned int round, unsigned long seq_no) {
    struct sockaddr_in clientaddr;
    int socketfd;
    Message *message;
    
    // cout << "PREPARE SEND\n" << flush;

    if (type == SYN) {
        // seq_no = this->seq_no;
        message = create_message(source_id, seq_no, content, type, ip, port, proposal_number, agreement, size, round);
    } else if (type == ACK) {
        message = create_message(source_id, seq_no, content, type, this->ip, this->port, proposal_number, agreement, size, round);
    } else {
        message = create_message(source_id, seq_no, content, type, ip, port, proposal_number, agreement, size, round);
    }

    if (message == NULL) {
        cout << "Cannot create the message\n" << flush;
        return;
    }

    
    // cout << "MEMSET ADDR SEND\n" << flush;

    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family        = AF_INET;
    clientaddr.sin_addr.s_addr   = ip;
    clientaddr.sin_port          = port;

    // cout << "BEFORE SEND\n" << flush;
    long int res = sendto(this->socket_fd, message, 
                offsetof(Message, content) + size, 0,
                reinterpret_cast<struct sockaddr*>(&clientaddr), 
                sizeof(clientaddr));

    // cout << "AFTER SEND SIZE " << res << endl << flush;
    if (res == -1) {
        return;
    }

    stringstream ack_key;
    ack_key << ipReadable(clientaddr.sin_addr.s_addr) << ":" << portReadable(clientaddr.sin_port);
    ack_key << "_" << message->seq_no << "_" << message->source_id;
    // cout << "Message key " << ack_key.str() << endl << flush;

    string key = ack_key.str();

    if (message->type == SYN || message->type == BROADCAST) {
        this->add_element_queue.lock();
        // cout << "Intraa " << message << endl << flush;
        // to_string(message);
        this->message_queue.insert({key, NOT_RECEIVED});
        
        // cout << "seteaza " << endl << flush;

        this->message_history.insert({key, message});
        // cout << "Adauga ce are de add "<< endl << flush;
        // this->seq_no += 1;
        this->add_element_queue.unlock();
        if (logging) {
            this->logger->log_send(this->seq_no);
        }
    } else if (message->type == RSYN) {
        this->add_element_queue.lock();
        this->message_queue.insert({ack_key.str(), NOT_RECEIVED});
        this->message_history.insert({ack_key.str(), message});
        this->add_element_queue.unlock();
    }

    // cout << "Finish to send " << endl << flush;
} 

Message* PerfectLink::receive(bool logging, unsigned int size) {
    struct sockaddr_in sourceaddr;
    Message *header = reinterpret_cast<Message *>(malloc(offsetof(Message, content)));
    if (header == NULL) return NULL;
    memset(&sourceaddr, 0, sizeof(sourceaddr));

    // cout << "RECEPTION" << endl << flush;
    socklen_t len = sizeof(sourceaddr);
    long int size_packet = recvfrom(this->socket_fd, header, offsetof(Message, content), MSG_PEEK , reinterpret_cast<struct sockaddr*>(&sourceaddr), 
                            &len);
    
    if (size_packet < 0) {
        return NULL;
    }

    Message *data_recv = NULL;
    if (header->content_size > 0) {
        data_recv = reinterpret_cast<Message *>(malloc(header->content_size + offsetof(Message, content)));
        // cout << "GET SIZE MESSAGE " << size_packet  << " " << header->content_size << endl << flush;
        if (data_recv == NULL) {
            return NULL;
        }

        long int res = recvfrom(this->socket_fd, data_recv, offsetof(Message, content) + header->content_size, 0, 
                                reinterpret_cast<struct sockaddr*>(&sourceaddr), 
                                &len);
        // cout << "FINAL PACKET SIZE "<< res << endl << flush;
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

    // cout << "DATA recv address " << data_recv << endl << flush;
    Message *recv_message = create_message(data_recv->source_id, data_recv->seq_no, data_recv->content,
                                                data_recv->type, data_recv->ip, data_recv->port,
                                                data_recv->proposal_number, data_recv->agreement, 
                                                data_recv->content_size, data_recv->round);
    

    // cout << "Create DATA recv" << endl << flush;
    if (data_recv != NULL) {
        free(data_recv);
        data_recv = NULL;
    }

    // cout << "DELETE data recv\n" << flush;

    if (header != NULL) {
        free(header); 
        header = NULL;
    }

    // cout << "DELETE header\n" << flush; 
    if (recv_message == NULL) {
        return NULL;
    }

    // cout << "Successfully recv data\n" << flush;

    if (recv_message == NULL || recv_message->source_id == 0) {
        cout << "error received message" << endl << flush;
        return NULL;
    }

    stringstream ack_key;

    // cout << "RECE" << endl<<flush;

    if (recv_message->type == ACK) {
        ack_key << ipReadable(recv_message->ip) << ":" << portReadable(recv_message->port);
        ack_key << "_" << recv_message->seq_no << "_" << recv_message->source_id;
        
        this->add_element_queue.lock();
        if (this->message_queue.find(ack_key.str()) != this->message_queue.end()) {
            this->message_queue[ack_key.str()] = DELETED;
        }
        this->add_element_queue.unlock();
    } else {
        ack_key << ipReadable(sourceaddr.sin_addr.s_addr) << ":" << portReadable(sourceaddr.sin_port);
        ack_key << "_" << recv_message->seq_no;

        this->send(sourceaddr.sin_addr.s_addr, sourceaddr.sin_port, NULL, ACK, false, recv_message->source_id, 
                   0, 0, ACKNOWLEGEMENT, recv_message->round);
        
        // if (this->enable_listener) {
            // cout << "ENABLED" << endl << flush;
            auto found = this->received_message.find(ack_key.str());
            if (found == this->received_message.end()) {
                this->received_message.insert({ack_key.str(), true});
                if (logging) {
                    this->logger->log_deliver(recv_message->source_id, recv_message->seq_no);
                }
            }
        // }
    }

    if (recv_message->type == SYN || recv_message->type == RSYN) {
        // cout << "CREATE RETURN MESSAGE " << endl << flush;
        // to_string(recv_message);
        // cout << "ADDRESS finish " <<recv_message <<endl << flush; 
        Message *returned_message = create_message(recv_message->source_id, recv_message->seq_no, recv_message->content,
                                                recv_message->type, recv_message->ip, recv_message->port,
                                                recv_message->proposal_number, recv_message->agreement, 
                                                recv_message->content_size, recv_message->round);
        // cout << "FINISH RETURN MESSAGE " << endl << flush;
        if (recv_message != NULL) {
            // if (recv_message->content != NULL)
            //     free(recv_message->content);
            free(recv_message);
            recv_message = NULL;
        }
        // cout << "FREE RETURN MESSAGE " << endl << flush;
        return returned_message;
    }
    
    if (recv_message != NULL) {
        // if (recv_message->content != NULL)
        //     free(recv_message->content);
        free(recv_message);
        recv_message = NULL;
    }
    return NULL;
}

unsigned long PerfectLink::getSeqNo() {
    return this->seq_no;
}

void PerfectLink::listen() {
    while(true) {
        this->receive(true, 0);
    }
}


void PerfectLink::cleanup() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_BEFORE_CLEANUP));
        this->add_element_queue.lock();
        auto iterator = this->message_queue.begin();
        auto end = this->message_queue.end();

        while (iterator != end) {
            if (iterator->second == DELETED) {
                string key = iterator->first;
                if (this->message_history[key] != NULL) {
                    free(this->message_history[key]);
                    this->message_history[key] = NULL;
                }
                this->message_history.erase(key);
                iterator = this->message_queue.erase(iterator);
                continue;
            }
            ++iterator;
        }
        this->add_element_queue.unlock();
    }
}

void PerfectLink::retry() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        this->add_element_queue.lock();
        unordered_map<string, ack_status> retryMessages(this->message_queue);
        this->add_element_queue.unlock();

        auto iterator = retryMessages.begin();
        auto end = retryMessages.end();
        
        while (iterator != end) {
            if (iterator->second == NOT_RECEIVED || iterator->second == NOT_SEND) {
                this->add_element_queue.lock();
                Message *aux_message = this->message_history[iterator->first];
                if (aux_message == NULL) {
                    ++iterator;
                    continue;
                }
                Message *message = create_message(aux_message->source_id, aux_message->seq_no, aux_message->content,
                                            RSYN, aux_message->ip, aux_message->port,
                                            aux_message->proposal_number, aux_message->agreement, aux_message->content_size,
                                            aux_message->round); 

                this->add_element_queue.unlock();

                if (message == NULL) {
                    cout << "cannot create message\n" << flush;
                    continue;
                }

                this->send(message->ip, message->port, message->content, 
                            RSYN, true, message->source_id, message->proposal_number,
                            message->content_size, message->agreement, 
                            message->round, message->seq_no);

                if (message != NULL) {  
                    free(message);
                    message = NULL;
                }
            }
            ++iterator;
        }
    }
}

unsigned long PerfectLink::getID() {
    return this->id;
}