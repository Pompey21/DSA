#include <chrono>
#include <iostream>
#include <thread>

#include "parser.hpp"
#include "hello.h"
#include "udp.hpp"
#include <signal.h>

std::ofstream outputFile;
UDPSocket udpSocket;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  for(auto const &output: udpSocket.get_logs()){
    outputFile << output << std::endl;
  }
  outputFile.close();

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  std::cout << "Works before";

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;
  unsigned long m,i;

  Parser parser(argc, argv);
  parser.parse();

  hello();
  std::cout << std::endl;

// PID = Process ID ?
  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";
  outputFile.open(parser.outputPath(), std::ofstream::out);
  if (!outputFile.is_open()){
    std::cout << "Cannot open the file..." << "\n";
    std::cout << "Exiting..." << "\n";
    exit(0);
  }

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";

  std::cout << "Doing some initialization...\n\n";

  std::cout << "Broadcasting and delivering messages...\n\n";

  std::ifstream config_file(parser.configPath());
  config_file >> m >> i;
  config_file.close();

  // create a socket for that given process!
  udpSocket = UDPSocket(hosts[parser.id()-1]);
  // start the socket -> we create two threads, one for sending and one for receiving
  udpSocket.create();

  // if this is not the receiving process, then it can broadcast the messages!
  if (parser.id() != i) {
    for (unsigned int message=1;message<=m;message ++) {
      udpSocket.enque(hosts[i-1], message);      
    }
  }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
