#include <chrono>
#include <iostream>
#include <thread>
#include <sstream>

#include "message.hpp"
#include "parser.hpp"
#include "file_logger.hpp"
#include "perfect_link.hpp"
#include "hello.h"
#include "lattice_agreement.hpp"
#include <signal.h>

Logger *logger;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n" << std::flush;
  logger->log_flush();

  // write/flush output file if necessary
  std::cout << "Writing output.\n" << std::flush;

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  std::cout << "START " << std::endl << std::flush;
  signal(SIGTERM, stop);
  signal(SIGINT, stop);
  // signal(SIGSEGV, stop);

  std::cout << "After initialize signals " << std::endl << std::flush;

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  std::cout << "After parsing " << std::endl << std::flush;

  // hello();
  // std::cout << std::endl;

  // std::cout << "My PID: " << getpid() << "\n";
  // std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
  //           << getpid() << "` to stop processing packets\n\n";

  // std::cout << "My ID: " << parser.id() << "\n\n";

  // std::cout << "List of resolved hosts is:\n";
  // std::cout << "==========================\n";

  std::cout << "Doing some initialization...\n\n" << std::flush;

  std::cout << "Broadcasting and delivering messages...\n\n" << std::flush;
  int number_messages;
  unsigned long receiver_id;

  // ifstream config_file; 
  // config_file.open(parser.configPath());
  // config_file >> number_messages;
  // config_file >> receiver_id;
  // config_file.close();

  logger = new Logger(parser.outputPath());

  auto hosts = parser.hosts();
  int id = 1;

  PerfectLink *perfect_link = new PerfectLink(hosts[parser.id() - 1].ip, hosts[parser.id() - 1].port, parser.id(), logger, false);
  Lattice_Agreement *agreement = new Lattice_Agreement(parser.configPath(), hosts, perfect_link);
 
  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}
