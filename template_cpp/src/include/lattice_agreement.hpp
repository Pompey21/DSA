# pragma once

#include <set>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <algorithm>
#include <bits/stdc++.h>

#include "parser.hpp"
#include "file_logger.hpp"
#include "perfect_link.hpp"

class Lattice_Agreement {
    public:
        Lattice_Agreement(std::string filename, std::vector<Parser::Host> hosts, Perfect_Link *perfect_link);
        ~Lattice_Agreement();

        void start_service();

        void propose();
        void receive();
        void first_proposal();

        void broadcast();

        void decide();
        void retry_propose();
        void read_file();

        // add some getters here

    private:
        bool active;
        unsigned int ack_count;
        unsigned int nack_count;
        int active_proposal_number;
        unsigned int f;
        unsigned int p, vs, ds;
        std::ifstream input_file;

        // Keep track of messages per round
        // map : round_number => set
        std::map<int, std::set<int>> proposed_values;
        std::map<int, std::set<int>> accepted_values;

        Perfect_Link *perfect_link;
        std::vector<Parser::Host> hosts;
        File_Logger *logger;
        std::mutex serialize;
        int *content;
        unsigned long sequence_number;
        unsigned int round;
};