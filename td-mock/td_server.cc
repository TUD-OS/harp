#include <exception>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <fstream>
#include <map>
#include <vector>
#include <utility>
#include <cmath>

#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "proto/tetris.pb.h"
#include "util/socket.h"
#include "util/string_util.h"
#include "util/debug_util.h"
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

    std::map<int, int> get_td_scores()
    {
        /* Return for each thread the ipc-class as determined by Intel TD hardware component */
        std::map<int, int> result;

        std::stringstream path;
        path << "/proc/" << _pid << "/task/";

        for (const auto & entry : std::filesystem::directory_iterator(path.str())) {
            std::fstream ipcc_file(entry.path() / "ipcc");
            auto tid = std::stod(entry.path().filename().string());

            if (ipcc_file.is_open()) {
                int ipcc;
                ipcc_file >> ipcc;
                ipcc_file.close();

                result[tid] = ipcc;
            } else {
                result[tid] = -1;
            }
        }

        return result;
    }

    std::map<int, bool> get_running()
    {
        /* Return for each thread the ipc-class as determined by Intel TD hardware component */
        std::map<int, bool> result;

        std::stringstream path;
        path << "/proc/" << _pid << "/task/";

        for (const auto & entry : std::filesystem::directory_iterator(path.str())) {
            std::fstream stat_file(entry.path() / "stat");
            auto tid = std::stod(entry.path().filename().string());

            if (stat_file.is_open()) {
                std::string stat_line;
                std::getline(stat_file, stat_line);
                stat_file.close();

                auto elements = string_util::split(stat_line, ' ');
                result[tid] = elements[2] == "R";
            } else {
                result[tid] = false;
            }
        }

        return result;
    }

    std::map<int, int> get_cores()
    {
        /* Return for each thread the current core that it is running on */
        std::map<int, int> result;

        std::stringstream path;
        path << "/proc/" << _pid << "/task/";

        for (const auto & entry : std::filesystem::directory_iterator(path.str())) {
            std::fstream stat_file(entry.path() / "stat");
            auto tid = std::stod(entry.path().filename().string());

            if (stat_file.is_open()) {
                std::string stat_line;
                std::getline(stat_file, stat_line);
                stat_file.close();

                auto elements = string_util::split(stat_line, ' ');
                result[tid] = std::stod(elements[38]);
            } else {
                result[tid] = -1;
            }
        }

        return result;
    }
};

using ClientPtr = std::shared_ptr<Client>;

double calc_global_ipc(const std::map<int, std::vector<int>> &cpu_tid_assignment,
        const std::map<int, std::map<int, unsigned long>> &tid_ipc_per_core,
        int target_tid, int target_cpu)
{
    double sum = 0;
    unsigned long cnt = 0;

    for (auto &[cpu, tids] : cpu_tid_assignment) {
        unsigned long inner_sum = 1;
        unsigned long inner_cnt = 0;

        for (auto &tid : tids) {
            inner_sum *= tid_ipc_per_core.at(tid).at(cpu);
            inner_cnt++;
        }
        if (target_cpu == cpu) {
            inner_sum *= tid_ipc_per_core.at(target_tid).at(target_cpu);
            inner_cnt++;
        }

        if (inner_cnt != 0) {
            cnt++;
            sum += std::pow(static_cast<double>(inner_sum), 1/static_cast<double>(inner_cnt));
        }
    }

    return sum;
}

class Manager {
   private:
    using IPCInfo = std::map<int, unsigned long>;
    using TDScores = std::map<int, int>;

    std::map<int, ClientPtr> _clients;
    std::map<int, IPCInfo> _cpu_ipc_scores;
    std::map<int, TDScores> _client_tdscores;

    bool _needs_reschedule;

   public:
    Manager() : _needs_reschedule(false) {
        /* Read in the IPC data per class per cpu */
        std::ifstream ipc_scores("/proc/ipc_scores");
        if (ipc_scores.is_open()) {
            while (!ipc_scores.eof()) {
                std::string line;
                std::getline(ipc_scores, line);

                if (line.size() == 0)
                    break;

                auto elements = string_util::split(line, " ");
                auto cpu = elements[0];
                IPCInfo cpu_scores;
                for (int i = 1; i < elements.size(); ++i) {
                    auto score = elements[i];
                    cpu_scores[i-1] = std::stoul(score);
                }
                _cpu_ipc_scores[std::stod(cpu.substr(3))] = cpu_scores;
            }

            ipc_scores.close();
        }
    }

    void needs_reschedule() {
        _needs_reschedule = true;
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

                    _client_tdscores[fd] = cl->get_td_scores();
                  //  needs_reschedule();
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
                    /* Answer all messages from the client with ACK */
                    ServerResponse response{};
                    response.set_type(ServerResponse::ACKNOWLEDGE);

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
            _client_tdscores.erase(fd);
            _clients.erase(fd);
        }

        needs_reschedule();
    }

    void check_td_scores()
    {
        std::map<int, TDScores> new_tdscores;
        bool reschedule_needed = false;

        for (auto &[cid, c] : _clients) {
            if (c->pid() == -1)
                continue;

            new_tdscores[cid] = c->get_td_scores();
        }

        if (new_tdscores.size() != _client_tdscores.size()) {
            reschedule_needed = true;
        } else {
            for (auto &[cid, oldscores] : _client_tdscores) {
                if (reschedule_needed)
                    break;

                if (!new_tdscores.contains(cid)) {
                    reschedule_needed = true;
                    break;
                }

                auto newscores = new_tdscores[cid];
                if (newscores.size() != oldscores.size()) {
                    reschedule_needed = true;
                    break;
                }

                for (auto &[tid, score] : oldscores) {
                    if (!newscores.contains(tid)) {
                        reschedule_needed = true;
                        break;
                    }

                    if (score != newscores[tid]) {
                        reschedule_needed = true;
                        break;
                    }
                }
            }
        }

        _client_tdscores = new_tdscores;

        if (reschedule_needed)
            needs_reschedule();
    }

    void reschedule()
    {
        if (!_needs_reschedule)
            return;

        /* 1) Get from all clients the tid -> ipcc information */
        std::map<int, int> ipcc_all;
        for (auto &[_, client_ipcc] : _client_tdscores) {
            ipcc_all.insert(client_ipcc.begin(), client_ipcc.end());
        }
        /* 1b) Get from all clients the tid -> core information and remove threads that are sleeping */
        std::map<int, int> cores_all;
        for (auto &[_, c] : _clients) {
            if (c->pid() == -1)
                continue;

            for (auto &[tid, core] : c->get_cores()) {
                cores_all[tid] = core;
            }

            for (auto &[tid, running] : c->get_running()) {
                if (!running) {
                    logger->debug("Ignoring sleeping thread %d\n", tid);
                    ipcc_all.erase(tid);
                    cores_all.erase(tid);
                }
            }
        }

        /* 2) Save for each tid the ipc per core , also calculate the SF (speedup factor) for each tid*/
        std::map<int, std::map<int, unsigned long>> tid_ipc_per_core;
        std::map<int, double> tid_sf;

        for (auto &[tid, ipcc] : ipcc_all) {
            std::map<int, unsigned long> ipc_per_core;
            unsigned long highest = 0;
            unsigned long lowest = -1;
            for (auto &[core, ipc_scores] : _cpu_ipc_scores) {
                auto score = ipc_scores[ipcc];
                if (score > highest)
                    highest = score;
                if (score < lowest)
                    lowest = score;

                ipc_per_core[core] = score;
            }

            tid_ipc_per_core[tid] = ipc_per_core;
            tid_sf[tid] = static_cast<double>(highest) / static_cast<double>(lowest);
        }

        /* 3) Generate the list of tids and sort it by their SF value */
        std::vector<int> tids;
        for (auto &[tid, _] : tid_ipc_per_core) {
            tids.push_back(tid);
        }
        std::sort(tids.begin(), tids.end(), [&](int tid1, int tid2) -> bool { return tid_sf[tid1] > tid_sf[tid2]; });

        /* 4) Go from highest SF to lowest SF and assign cores to the threads */
        std::map<int, std::vector<int>> cpu_tid_assignment;
        std::vector<int> cores;
        for (auto &[core, _] : _cpu_ipc_scores) {
            cpu_tid_assignment[core] = {};
            cores.push_back(core);
        }

        for (auto &tid : tids) {
            /* Sort the cores by their ipc_scores */
            std::sort(cores.begin(), cores.end(), 
                    [&](int core1, int core2) -> bool { return tid_ipc_per_core[tid][core1] > tid_ipc_per_core[tid][core2]; });

            /* Start with the currently used core to minimize thread movement */
            int best_core = cores_all[tid];
            double best_score = calc_global_ipc(cpu_tid_assignment, tid_ipc_per_core, tid, best_core);

            for (auto core : cores) {
                auto cur_score = calc_global_ipc(cpu_tid_assignment, tid_ipc_per_core, tid, core);

                if (cur_score > best_score) {
                    best_score = cur_score;
                    best_core = core;
                }
            }

            if (best_core == cores_all[tid])
                logger->debug("Leaving %d at %d (Score: %lf)\n", tid, best_core, best_score);
            else
                logger->debug("Moving %d to %d (Score: %lf)\n", tid, best_core, best_score);

            cpu_tid_assignment[best_core].push_back(tid);
        }

        /* Do the actual migration of threads to their assigned CPUs */
        for (auto &[cpu, tids] : cpu_tid_assignment) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpu, &cpuset);

            for (auto tid : tids)
                sched_setaffinity(tid, sizeof(cpuset), &cpuset);
        }

        _needs_reschedule = false;
    }
};


void usage() {
  std::cout << "usage: tetris_td [-h]\n"
            << "\n"
            << "Options:\n"
            << "   -h, --help                show this help message.\n"
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
  sigaddset(&sigmask, SIGALRM);

  /* First block the signals. */
  sigprocmask(SIG_BLOCK, &sigmask, nullptr);

  /* And create a signal fd where these signals are managed. */
  int sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  return sig_fd;
}

bool setup_timer() {
  timer_t timerid;
  struct sigevent sev;
  struct itimerspec its;

  /* Prepare and create the timer signal */
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;

  int ret = timer_create(CLOCK_REALTIME, &sev, &timerid);
  if (ret == -1) {
      std::cerr << "Failed to create timer" << std::endl
                << strerror(errno) << std::endl;
      return false;
  }

  /* Arm the timer */
  its.it_value.tv_sec = 1;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;

  ret = timer_settime(timerid, 0, &its, NULL);
  if (ret == -1) {
      std::cerr << "Failed to arm timer" << std::endl
                << strerror(errno) << std::endl;

      return false;
  }

  return true;
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

          switch (siginfo.ssi_signo) {
          case SIGALRM:
            manager.check_td_scores();
            break;
          default:
            logger->info("Received a signal (%i)\n", siginfo.ssi_signo);
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

    manager.reschedule();
  }
}

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};

    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else {
      usage();
      return 1;
    }
  }

  std::cout << "Welcome the TETRiS TD" << std::endl;

  /* Setup logging */
  logger = debug::Logger::get();

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

  if (!setup_timer()) {
    return 1;
  }

  /* The event loop */
  event_loop(manager, epoll_fd, server_fd, control_fd, sig_fd);

  std::cout << "Exiting" << std::endl;
  ::close(sig_fd);

  return 0;
}
