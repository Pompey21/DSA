#include "message.hpp"
#include "parser.hpp"
#include "file_logger.hpp"
#include "perfect_link.hpp"
#include "hello.h"
#include "lattice_agreement.hpp"

#include <signal.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <sstream>

File_Logger *file_logger;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n" << std::flush;
  file_logger->log_flush();

  // write/flush output file if necessary
  std::cout << "Writing output.\n" << std::flush;

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  std::cout << "Starting the process... " << std::endl << std::flush;
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  std::cout << "Parsing the information..." << std::endl << std::flush;

  Parser parser(argc, argv);
  parser.parse();


  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";

  std::cout << "Doing some initialization...\n\n" << std::flush;

  std::cout << "Broadcasting and delivering messages...\n\n" << std::flush;
  int number_messages;
  unsigned long receiver_id;

  file_logger = new File_Logger(parser.outputPath());

  auto hosts = parser.hosts();
  int id = 1;

  Perfect_Link *perfect_link = new Perfect_Link(hosts[parser.id() - 1].ip, hosts[parser.id() - 1].port, parser.id(), file_logger, false);
  Lattice_Agreement *agreement = new Lattice_Agreement(parser.configPath(), hosts, perfect_link);
 
  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}
