#include "lattice_agreement.hpp"

Lattice_Agreement::Lattice_Agreement(std::string filename, std::vector<Parser::Host> hosts, Perfect_Link *perfect_link) {
    // initialisation of the agreement -> same as in the article provided
    this->active = false;
    this->ack_count = 0;
    this->nack_count = 0;
    this->active_proposal_number = 0;
    this->hosts = hosts;
    this->perfect_link = perfect_link;
    this->sequence_number = 1;
    this->round = 0;
    this->f = static_cast<unsigned int>((hosts.size() - 1) / 2);
    this->input_file.open(filename);
    if (this->input_file.is_open()) {
        this->input_file >> this->p;
        this->input_file >> this->vs;
        this->input_file >> this->ds;
        std::string line;
        getline(this->input_file, line);
    } else {
        perror("Config file does not exist!");
        exit(1);
    }

    this->content = reinterpret_cast<int *>(calloc(2 * (this->ds + 1), sizeof(int)));
    this->start_service();
}

Lattice_Agreement::~Lattice_Agreement() {
    if (this->input_file.is_open()) {
        this->input_file.close();
    }

    if (this->content != NULL) {
        free(this->content);
    }
}

void Lattice_Agreement::start_service() {
    this->first_proposal();
    std::thread propose(&Lattice_Agreement::proposal, this);
    std::thread reception(&Lattice_Agreement::reception, this);
    std::thread decide(&Lattice_Agreement::decide, this);
    std::thread retry_proposal(&Lattice_Agreement::retry_proposal, this);

    propose.detach();
    reception.detach();
    decide.detach();
    retry_proposal.detach();
}

void Lattice_Agreement::broadcast() {
    int *data = this->content;
    data[0] = static_cast<int>(this->proposed_values[this->round].size());
    int index = 1;

    for (auto iter : this->proposed_values[this->round]) {
        data[index] = iter;
        index++;
    }

    for (auto host : this->hosts) {
        this->perfect_link->send(host.ip, host.port, data, SYN, false, this->perfect_link->getID(), 
                                 this->active_proposal_number, static_cast<unsigned int>(sizeof(int) * (data[0] + 1)), 
                                 PROPOSAL, this->round, this->sequence_number);
    }
}

void Lattice_Agreement::read_from_file() {
    std::string line;
    getline(this->input_file, line);
    std::istringstream ss(line);
    int number;
    while (ss >> number) {
        this->proposed_values[this->round].insert(number);
    }
    this->p --;
}

void Lattice_Agreement::first_proposal() {
 
        this->serialize.lock();
        std::cout << "PROPOSAL" << std::endl << std::flush;
        if (this->p > 0 && !this->active) {
            this->active = true;
            this->active_proposal_number += 1;
            this->ack_count = 0;
            this->nack_count = 0;

            // cout << "Start reading from file" << endl << flush;
            // proposed_value𝑖 ← proposal
            this->read_from_file();
            // broadcast (PROPOSAL, SET proposed_value, INTEGER active_proposal_number))
            
            // cout << "Start broadcasting" << endl << flush;
            this->broadcast();
            this->sequence_number++;
        }
        this->serialize.unlock();
}


void Lattice_Agreement::proposal() {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        this->serialize.lock();
        // cout << "PROPOSAL" << endl << flush;
        if (this->p > 0 && !this->active) {
            this->active = true;
            this->active_proposal_number += 1;
            this->ack_count = 0;
            this->nack_count = 0;

            // cout << "Start reading from file" << endl << flush;
            // proposed_value𝑖 ← proposal
            this->read_from_file();
            // broadcast (PROPOSAL, SET proposed_value, INTEGER active_proposal_number))
            
            // cout << "Start broadcasting" << endl << flush;
            this->broadcast();
            this->sequence_number++;
        } 
        this->serialize.unlock();
    }
}

void Lattice_Agreement::retry_proposal() {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        this->serialize.lock();
        if (this->active && this->nack_count > 0) {
            this->active_proposal_number += 1;
            this->ack_count = 0;
            this->nack_count = 0;
            this->broadcast();
        }
        
        this->serialize.unlock();
    }
}


void Lattice_Agreement::reception() {
    while (1) {
        Message *message = this->perfect_link->receive(false, static_cast<unsigned int>((this->ds + 1) * sizeof(int)));
        
        if (message == NULL) {
            continue;
        }

        this->serialize.lock();
        if (message->agreement == ACKNOWLEGEMENT && message->proposal_number == this->active_proposal_number) {
            this->ack_count += 1;
        } else if (message->agreement == NACK && message->proposal_number == this->active_proposal_number) {
            // proposed_value ← proposed_value ∪ value
            int *value = reinterpret_cast<int *>(message->content);
            for (int i = 1; i <= value[0]; i ++) {
                this->proposed_values[message->round].insert(value[i]);
            }
            this->nack_count += 1;
        } else if (message->agreement == PROPOSAL) {
            /* upon reception of ⟨proposal, Set proposed_value, Integer proposal_number⟩ from proposer 𝑃𝑗 such that
                accepted_value𝑖 ⊆ proposed_value:
                accepted_value𝑖 ← proposed_value
                send ⟨ack, proposal_number⟩ to 𝑃𝑗
            */
            /* upon reception of ⟨proposal, Set proposed_value, Integer proposal_number⟩ from proposer 𝑃𝑗 such that
                accepted_value𝑖 ⊈ proposed_value:
                accepted_value𝑖 ← accepted_value𝑖 ∪ proposed_value
                send ⟨nack, proposal_number, accepted_value𝑖⟩ to 𝑃𝑗
            */
            
            int *value = reinterpret_cast<int *>(message->content);

            bool include_set = includes(value + 1, value + value[0] + 1, 
                                        this->accepted_values[message->round].begin(), 
                                        this->accepted_values[message->round].end());

            if (include_set) {
                this->accepted_values[message->round].insert(value + 1, value + value[0] + 1);

                this->perfect_link->send(this->hosts[message->source_id - 1].ip, this->hosts[message->source_id - 1].port,
                                         NULL, SYN, false, this->perfect_link->getID(), message->proposal_number, 0,
                                         ACKNOWLEGEMENT, message->round, this->sequence_number);
                this->sequence_number ++;
            } else {
                this->accepted_values[message->round].insert(value + 1, value + value[0] + 1);

                int *send_values = &(this->content[this->ds + 1]);
                send_values[0] = static_cast<int>(this->accepted_values[message->round].size());
                int index = 1;
                for (auto iter : this->accepted_values[message->round]) {
                    send_values[index] = iter;
                    index++;
                }

                this->perfect_link->send(this->hosts[message->source_id - 1].ip, this->hosts[message->source_id - 1].port, 
                                         send_values, SYN, false, this->perfect_link->getID(), message->proposal_number,
                                         static_cast<unsigned int>(sizeof(int) * (send_values[0] + 1)), 
                                         NACK, message->round, this->sequence_number);
                this->sequence_number++;
            }
        }
    
        this->serialize.unlock();
    }
}


void Lattice_Agreement::decide() {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  
        this->serialize.lock();
        if (this->active && (this->ack_count) >= (this->f + 1)) {
            // trigger decide -> means to implement a logger
            this->perfect_link->logger->log_decision(this->proposed_values[this->round]);
            this->active = false;
            this->round++;
        }
        this->serialize.unlock();
    }
}

