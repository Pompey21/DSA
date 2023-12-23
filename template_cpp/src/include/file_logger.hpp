# pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <bits/stdc++.h>

// In summary, the Logger class is designed to facilitate 
// logging of events in a concurrent or distributed system.

class File_Logger {
    public:
        File_Logger(std::string filename) {
            this->log_file.open(filename);
            if (!this->log_file) {
                perror("Could not open the output file. Exiting with an error.");
                exit(EXIT_FAILURE);
            }
        }
        ~File_Logger() {
            if (this->log_file) {
                this->log_file.close();
            }
        }

        // void log_deliver

        void log_deliver(unsigned long sender, unsigned long sequence_number) {
            this->write_lock.lock();
            this->cached_logs << "d " << sender << " " << sequence_number << std::endl;
        }

        void log_broadcast(unsigned long sequence_number) {
            this->write_lock.lock();
            this->cached_logs << "b " << sequence_number << std::endl;
            this->write_lock.unlock();
        }

        void log_decision(std::set<int> line) {
            this->write_lock.lock();
            auto iterator = line.begin();
            while (iterator != line.end()) {
                this->cached_logs << *iterator;
                if (++iterator != line.end()) {
                    this->cached_logs << " ";
                } else {
                    this->cached_logs << "\n";
                }
            }
            this->write_lock.unlock();
        }

        void log_flush() {
            this->write_lock.lock();
            if (this->log_file) {
                this->log_file << this->cached_logs.str();
                this->log_file.flush();
            }
            this->write_lock.unlock();
        }

    private:
        std::ofstream log_file;
        std::stringstream cached_logs;
        std::mutex write_lock;
};