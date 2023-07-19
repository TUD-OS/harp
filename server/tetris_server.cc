#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <signal.h>

#include "manager.h"
#include "util/socket.h"

/***
 * Global variables
 ***/

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;

void usage()
{
    std::cout 
      << "usage: tetrisserver [-h]" << std::endl
      << std::endl
      << "Options:" << std::endl
      << "   -h, --help           show this help message." << std::endl
      << std::endl;
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
Socket setup_socket(const std::string& socket_path, int& fd) {
  Socket sock;
  try {
    sock.open(socket_path);
    sock.non_blocking();
    sock.listening();
    fd = sock.fd();
  } catch (const std::runtime_error& e) {
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
int setup_epoll(const std::vector<int>& fds) {
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    std::cerr << "Failed to initialize epoll." << std::endl
              << strerror(errno) << std::endl;
    return -1;
  }

  epoll_event e;
  for (auto& fd : fds) {
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

void manage_event_loop(int epoll_fd, int server_fd, int control_fd, int sig_fd,
                       Manager& manager) {
  // Code for managing event loop
  epoll_event events[MAXEVENTS];
  bool done = false;

  while (!done) {
    int n;

    n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

    for (int i = 0; i < n; ++i) {
      epoll_event* cur = &events[i];

      if (cur->data.fd == server_fd) {
        /* There are a new connections at the server socket.
         * Connect with all of them. */
        while (1) {
          sockaddr_un in_sock;
          socklen_t in_sock_size = sizeof(in_sock);
          int infd =
              ::accept(cur->data.fd, reinterpret_cast<sockaddr*>(&in_sock),
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
              ::accept(cur->data.fd, reinterpret_cast<sockaddr*>(&in_sock),
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
            case SIGUSR1:
              manager.update_mappings();
              break;
            case SIGUSR2:
              manager.print_mappings();
              break;
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

int main(int argc, char* argv[]) {
  /* Parsing command line arguments. */
  std::string mappings_path;

  if (argc > 2) {
    usage();
    return 1;
  } else if (argc == 1) {
    mappings_path = path_util::getcwd();
  } else {
    std::string arg{argv[1]};
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else {
      usage();
      return 1;
    }
  }

  std::cout << "Welcome to TETRiS" << std::endl;

  /* Setup logging */
  logger = debug::Logger::get();

  /* Setting up the manager */
  Manager manager{};

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
  manage_event_loop(epoll_fd, server_fd, control_fd, sig_fd, manager);

  std::cout << "Exiting" << std::endl;
  ::close(sig_fd);

  return 0;
}
