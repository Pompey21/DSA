# pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <bits/stdc++.h>

class Logger {
    public:
        Logger(std::string filename) {
            this->log_file.open(filename);
            if (!this->log_file) {
                perror("Failed to open the output file");
                exit(EXIT_FAILURE);
            }
        }
        ~Logger() {
            if (this->log_file) {
                this->log_file.close();
            }
        }

        void log_deliver(unsigned long sender, unsigned long seqno) {
            this->write_logs.lock();
            this->cached_logs << "d " << sender << " " << seqno << std::endl;
        }

        void log_send(unsigned long seqno) {
            this->write_logs.lock();
            this->cached_logs << "b " << seqno << std::endl;
            this->write_logs.unlock();
        }

        void log_decision(std::set<int> line) {
            this->write_logs.lock();
            auto iterator = line.begin();
            while (iterator != line.end()) {
                this->cached_logs << *iterator;
                if (++iterator != line.end()) {
                    this->cached_logs << " ";
                } else {
                    this->cached_logs << "\n";
                }
            }
            this->write_logs.unlock();
        }

        void log_flush() {
            this->write_logs.lock();
            if (this->log_file) {
                this->log_file << this->cached_logs.str();
                this->log_file.flush();
            }
            this->write_logs.unlock();
        }

    private:
        std::ofstream log_file;
        std::stringstream cached_logs;
        std::mutex write_logs;
};