#include <chrono>
#include <iostream>
#include <thread>
#include <sstream>

#include "message.hpp"
#include "parser.hpp"
#include "logger.hpp"
#include "perfect_link.hpp"
#include "hello.h"
#include "lattice.hpp"
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
  Agreement *agreement = new Agreement(parser.configPath(), hosts, perfect_link);
  // URB *urb = new URB(perfect_link, hosts, number_messages);
  // if (number_messages * hosts.size() > 200) {
  //   number_messages = static_cast<int>(200 / hosts.size());
  //   number_messages = number_messages <= 1 ? 2 : number_messages;
  // }
  // cout << "Start sending " << endl << flush;
  // for (int i = 0; i < number_messages; i++) {
  //   urb->broadcast(reinterpret_cast<void *>(NULL));
  // }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}
