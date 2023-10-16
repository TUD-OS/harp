#include <exception>
#include <filesystem>
#include <stdexcept>

#include <memory>
#include <signal.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "util/platform/reader.h"
#include "util/platform/platform.h"
#include "util/socket.h"
#include "util/string_util.h"
#include "util/protobuf_util.h"
#include "util/connection.h"


using namespace tetris;

/***
 * Global variables
 ***/

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;

using ConnectionPtr = std::shared_ptr<Connection>;

/***
 * Support Classes
 ***/
class Client {
   private:
    int _pid;
    std::string _exec;
    ConnectionPtr _connection;

    friend class Manager;

    std::string push_listener_path()
    {
        if (_pid == -1) {
            logger->error("Push Listener Path not fully configured!\n");
            throw std::runtime_error("Can't send messages to not fully registered client!\n");
        }

        std::stringstream ss;
        ss << "/tmp/tetris_push_listener_" << _pid;
        return ss.str();
    }

   public:
    Client(ConnectionPtr connection) :
        _pid{-1}, _exec{}, _connection{connection}
    {}

    void pid(int p)
    {
        _pid = p;

        /* Also update the push_listener_path */
    }

    int pid() const
    {
        return _pid;
    }

    std::string exec() const
    {
        return _exec;
    }

    void exec(const std::string e)
    {
        _exec = e;
    }

    void send_mapping(const std::vector<std::string> &cpus)
    {
        MockServerMessage msg;
        Connection conn{push_listener_path()};
        logger->info("Sending client '%s' [%d] mapping info\n", _exec.c_str(), _pid);

        msg.set_type(MockServerMessage::ACTIVATE_OP);
        auto op_info = msg.mutable_activated_op_info();

        for (auto c : cpus) {
            op_info->add_cpus(c);
        }

        protobuf_util::Send(conn.locked(), msg);

        ClientResponse response{};
        protobuf_util::Receive(conn.locked(), response);

        if (response.type() != ClientResponse::ACKNOWLEDGE) {
            logger->warning("Client failed to acknowledge message!\n");
        }
    }
};

using ClientPtr = std::shared_ptr<Client>;
using PlatformPtr = std::unique_ptr<Platform>;

class Manager {
   private:
    std::map<int, ClientPtr> _clients;
    std::vector<std::string> _available_cpus;
    PlatformPtr _platform;

   public:
    Manager(std::unique_ptr<Platform>&& platform, const std::vector<std::string> &cpu_list)
        : _clients{}, _available_cpus{}, _platform(std::move(platform))
    {
        for (auto c : cpu_list) {
            auto cptr = _platform->FindCPUThread(c);
            if (cptr) {
                _available_cpus.push_back(c);
                logger->debug("Enabling CPU %s (%d)\n", c.c_str(), cptr->GetID());
            } else {
                logger->warning("Unknown CPU %s specified -- ignoring\n", c.c_str());
            }
        }

        if (_available_cpus.size() == 0) {
            logger->debug("All CPUs are enabled!\n");
            for (const auto &c : _platform->GetCPUThreads()) {
                _available_cpus.push_back(c.second->GetName());
            }
        } else {
            logger->debug("Using CPUs: %s\n", string_util::join(_available_cpus, ",").c_str());
        }
    }

    void client_connect(int fd, ConnectionPtr conn)
    {
        auto c = _clients.find(fd);
        if (c != _clients.end()) {
            logger->warning("Reconnection from client %d\n", fd);
        } else {
            _clients[fd] = std::make_shared<Client>(conn);
            logger->debug("Connection from client %d\n", fd);
        }
    }

    bool client_message(int fd)
    {
        auto c = _clients.find(fd);
        if (c == _clients.end()) {
            logger->warning("Message from unknown client %d\n", fd);
            return true;
        }

        logger->debug("Message for client %d\n", fd);
        bool done = false;
        bool close = false;
        ClientPtr cl = c->second;

        while (!done) {
            if (cl->_pid == -1) {
                  /* The client is not yet fully registered with the server. Until now we only accept
                   * registration requests. */
                  RegistrationRequest request{};
                  auto res = protobuf_util::Receive(cl->_connection->locked(), request);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    cl->_pid = request.pid();
                    cl->_exec = request.exec();

                    logger->info(" -> The client registered! '%s' [%d]\n", cl->_exec.c_str(), cl->_pid);

                    /* Construct and send the server's registration response */
                    RegistrationResponse response{};
                    response.set_id(fd);

                    if (protobuf_util::Send(cl->_connection->locked(), response) != Connection::OutState::DONE)
                        logger->error("Failed to acknowledge the new-thread message\n");

                    /* Directly inform the client about the preferred mapping */
                    cl->send_mapping(_available_cpus);
                }
            } else {
                  /* The client is fully registered, however, we ignore messages */
                ClientMessage msg{};
                auto res = protobuf_util::Receive(cl->_connection->locked(), msg);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    ServerResponse response{};
                    response.set_type(ServerResponse::ERROR);

                    protobuf_util::Send(cl->_connection->locked(), response);
                }
            }
        }

        return close;
    }

    void client_disconnect(int fd)
    {
        auto c = _clients.find(fd);
        if (c == _clients.end()) {
            logger->debug("Unknown client disconnected\n");
        } else {
            logger->debug("Client %s [%d] disconnected\n", c->second->exec().c_str(), c->second->pid());
            _clients.erase(c);
        }
    }
};


void usage() {
  std::cout << "usage: tetris_mock [-h] [-p platform] CPUS\n"
            << "\n"
            << "Options:\n"
            << "   -h, --help                show this help message.\n"
            << "   -p, --platform <name>     specify the platform.\n"
            << "Positionals:\n"
            << "   CPUS                      the available cpus for the managed client (',' or ' ' separated)\n"
            << "\n";
}

/**
 * \brief Setup a socket with a given path and return its file descriptor.
 *
 * If any internal operation fails, the function writes an error message to the
 * standard error output and terminates the program.
 *
 * \param socket_path The path where to open the socket.
 * \param fd Reference to an integer where to store the file descriptor of the
 *           opened socket.
 * \return The created and setup Socket object.
 */
Socket setup_socket(const std::string &socket_path, int &fd) {
  Socket sock;
  try {
    sock.open(socket_path);
    sock.non_blocking();
    sock.listening();
    fd = sock.fd();
  } catch (const std::runtime_error &e) {
    std::cerr << "Failed to open socket " << socket_path << "\n"
              << e.what() << "\n";
    exit(1);
  }
  return sock;
}

/**
 * \brief Setup signal handling for the program.
 *
 * If creating the signal file descriptor fails, the function returns -1.
 *
 * \return The file descriptor of the created signal file descriptor, or -1 on
 * failure.
 */
int setup_signal_handling() {
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGABRT);
  sigaddset(&sigmask, SIGHUP);
  sigaddset(&sigmask, SIGINT);
  sigaddset(&sigmask, SIGQUIT);
  sigaddset(&sigmask, SIGTERM);
  sigaddset(&sigmask, SIGUSR1);
  sigaddset(&sigmask, SIGUSR2);

  /* First block the signals. */
  sigprocmask(SIG_BLOCK, &sigmask, nullptr);

  /* And create a signal fd where these signals are managed. */
  int sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  return sig_fd;
}

/**
 * \brief Sets up the epoll event loop.
 *
 * \param fds A vector containing file descriptors to be added to the epoll
 * loop.
 *
 * \return On success, it returns the file descriptor for the new epoll
 * instance. On failure, it returns -1 and prints an error message.
 */
int setup_epoll(const std::vector<int> &fds) {
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    std::cerr << "Failed to initialize epoll." << std::endl
              << strerror(errno) << std::endl;
    return -1;
  }

  epoll_event e;
  for (auto &fd : fds) {
    e.data.fd = fd;
    e.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
      std::cerr << "Failed to add socket " << fd << " to epoll." << std::endl
                << strerror(errno) << std::endl;
      return -1;
    }
  }
  return epoll_fd;
}

void event_loop(Manager &manager, int epoll_fd, int server_fd, int control_fd, int sig_fd) {
  // Code for managing event loop
  epoll_event events[MAXEVENTS];
  bool done = false;

  while (!done) {
    int n;

    n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

    for (int i = 0; i < n; ++i) {
      epoll_event *cur = &events[i];

      if (cur->data.fd == server_fd) {
        /* There are a new connections at the server socket.
         * Connect with all of them. */
        while (1) {
          sockaddr_un in_sock;
          socklen_t in_sock_size = sizeof(in_sock);
          int infd =
              ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock),
                       &in_sock_size);
          if (infd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
              /* We connected to all possible connections already.
               * Continue with the main loop. */
              break;
            } else {
              logger->error(
                  "An error happened while accepting a connection: %s",
                  strerror(errno));
              break;
            }
          }

          /* Make the new socket non-blocking and add it to epoll. */
          ConnectionPtr in_conn = std::make_shared<Connection>(infd, in_sock);
          in_conn->non_blocking();

          epoll_event e;
          e.data.fd = infd;
          e.events = EPOLLIN;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &e) == -1) {
            logger->error("Failed to add new connection to epoll: %s",
                          strerror(errno));
            ::close(infd);
          } else {
            logger->info("A new client connected (%i)\n", infd);

            manager.client_connect(infd, in_conn);
          }
        }
      } else if (cur->data.fd == control_fd) {
        /* There are a new connections at the control socket.
         * Connect with all of them. */
        while (1) {
          sockaddr_un in_sock;
          socklen_t in_sock_size = sizeof(in_sock);
          int infd =
              ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock),
                       &in_sock_size);
          if (infd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
              /* We connected to all possible connections already.
               * Continue with the main loop. */
              break;
            } else {
              logger->error(
                  "An error happened while accepting a connection: %s",
                  strerror(errno));
              break;
            }
          }

          /* Control connection are usually single shot. So just open this
           * connection and directly read out the data */

          /* TODO: Handle control messages */
        }
      } else if (cur->data.fd == sig_fd) {
        /* There was a signal delivered to this process. */
        while (1) {
          signalfd_siginfo siginfo;
          ssize_t count;

          count = read(sig_fd, &siginfo, sizeof(siginfo));
          if (count == -1) {
            if (errno != EAGAIN)
              logger->error(
                  "An error happened while reading data from signal fd: %s",
                  strerror(errno));

            break;
          }

          logger->info("Received a signal (%i)\n", siginfo.ssi_signo);

          switch (siginfo.ssi_signo) {
          default:
            done = 1;
          }
        }
      } else if (cur->events & EPOLLIN) {
        /* Some client tried to send us data. */
        logger->debug("The client sent a message\n");

        if (manager.client_message(cur->data.fd)) {
          manager.client_disconnect(cur->data.fd);
        }
      } else if (cur->events & EPOLLHUP) {
        /* Some client disconnected. */
        manager.client_disconnect(cur->data.fd);
      } else {
        logger->warning("Strange event at %i\n", cur->data.fd);
        ::close(cur->data.fd);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  /* Parsing command line arguments. */
  std::string platform_path;
  std::vector<std::string> cpu_list;

  for (int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};

    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--platform" || arg == "-p") {
      if (i + 1 >= argc) {
        std::cerr << "Expected platform name or path after " << arg << ".\n";
        usage();
        return 1;
      }

      i++;
      std::string platform = argv[i];

      if (string_util::ends_with(platform, ".yaml")) {
        if (!std::filesystem::exists(platform)) {
          std::cerr << "Specified .yaml platform file does not exist.\n";
          return 1;
        } else {
          platform_path = platform;
        }
      } else {
        // Check if the platform file exists in the default location
        std::string default_path =
            "../examples/platforms/platform_" + platform + ".yaml";
        if (std::filesystem::exists(default_path)) {
          platform_path = default_path;
        } else {
          std::cerr << "Specified platform is not available and it's not a "
                       "valid .yaml file path.\n";
          return 1;
        }
      }
    } else {
        /* Next arguments are the cpu list */
        auto cpus = string_util::split(arg, ",");
        for (const auto &c : cpus) {
            cpu_list.push_back(c);
        }
    }
  }

  if (platform_path.empty()) {
    std::cerr << "Platform not specified.\n";
    usage();
    return 1;
  }

  if (cpu_list.empty()) {
    std::cerr << "No CPUs specified, assuming all can be used!" << std::endl;
  }

  std::cout << "Welcome the TETRiS MOCK Server" << std::endl;

  /* Setup logging */
  logger = debug::Logger::get();

  /* Create a platform */
  auto reader = YamlPlatformReader();
  auto platform = reader.ReadFromFile(platform_path);

  /* Setting up the manager */
  Manager manager{std::move(platform), cpu_list};

  // Setting up the server and control sockets
  int server_fd = -1;
  Socket server_sock = setup_socket(SERVER_SOCKET, server_fd);

  int control_fd = -1;
  Socket control_sock = setup_socket(CONTROL_SOCKET, control_fd);

  logger->info(" * Server socket: %s (%i)\n", server_sock.path(), server_fd);
  logger->info(" * Control socket: %s (%i)\n", control_sock.path(), control_fd);

  /* Setup signal handling */
  int sig_fd = setup_signal_handling();
  if (sig_fd == -1) {
    std::cerr << "Failed to create signal fd." << std::endl
              << strerror(errno) << std::endl;
    return 1;
  }

  /* Setup the epoll event loop. */
  int epoll_fd = setup_epoll({server_fd, control_fd, sig_fd});
  if (epoll_fd == -1) {
    return 1;
  }

  /* The event loop */
  event_loop(manager, epoll_fd, server_fd, control_fd, sig_fd);

  std::cout << "Exiting" << std::endl;
  ::close(sig_fd);

  return 0;
}
