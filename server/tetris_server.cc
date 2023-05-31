#include "algorithm.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "filter.h"
#include "mapping.h"
#include "util/path_util.h"
#include "util/socket.h"
#include "util/string_util.h"
#include "util/tetris.h"
#include "proto/tetris.pb.h"
#include "util/protobuf_util.h"
#include "mapping_reader.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>



/***
 * Global variables
 ***/

using ConnectionPtr = std::shared_ptr<Connection>;

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;


/***
 * Failure handling for no mapping found
 ***/

class NoMappingError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


/***
 * Client Program
 ***/

class Client
{
public:
    struct Thread
    {
        bool named;
        std::string name;
        int tid;
        CPUList cpus;

        Thread(int tid, CPUList cpus) :
            named{false}, name{}, tid{tid}, cpus{cpus}
        {
            std::stringstream ss;
            ss << "thread-" << tid;
            name = ss.str();
        }

        Thread(int tid, const std::string &name, CPUList cpus) :
                named{true}, name{name}, tid{tid}, cpus{cpus}
        {}
    };

    /* The management type of an application */
    enum Type : int {
        /* PASSIV: the default type --> The application will be moved around as a
         * whole with no additional management. Mappings and characteristics
         * are not required for this type of application */
        PASSIV = 0x1,

        /* ACTIVE: the default managed type --> TETRiS will actively manage the application.
         * For this type a simple mapping has to be provided. TETRiS will try to optimize
         * the application depending on the application's criteria and the overall system state */
        ACTIVE = 0x2,

        /* PER_THREAD: a subtype of ACTIVE --> TETRiS will distinguish mappings that have different
         * assignments of threads to CPUs as different mappings and choose them according to
         * the application's otimization criteria. For this type an appropriate mapping has to
         * be provided by the client. */
        PER_THREAD = 0x4,

        /* SCALABLE: a subtype of ACTIVE --> TETRiS will send scaling information to the
         * application. For this type an appropriate mapping has to be provided by the client */
        SCALABLE = 0x8
    };

public:
    ConnectionPtr connection;
    std::string exec;
    int pid;
    std::vector<Thread> threads;

    std::vector<Mapping> mappings;
    Mapping active_mapping;

    std::string push_listener_path;

    int type;

private:
    /* Internal interface */
    void new_thread(int tid)
    {
        logger->info("New thread 'unnamed' [%i] registered for client '%s' [%d]\n", tid, exec.c_str(), pid);

        auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
        if (it == threads.end()) {
            /* With unnamed threads we don't support per-thread CPU assignments */
            threads.emplace_back(tid, active_mapping.cpus);

            /* Instead we assign the thread the CPUs of the mapping and let it migrate around within the assigned CPUs */
            logger->info(" * enabled cpu(s) %s\n", string_util::join(active_mapping.cpus.cpulist(num_cpus), ",").c_str());

            cpu_set_t cset = active_mapping.cpus.cpu_set();
            sched_setaffinity(tid, sizeof(cset), &cset);
        } else {
            logger->warning("Duplicate thread 'unnamed' [%i]\n", tid);
        }
    }

    void new_thread(int tid, const std::string &name)
    {
        logger->info("New thread '%s' [%i] registered for client '%s' [%d]\n", name.c_str(), tid, exec.c_str(), pid);

        auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
        if (it == threads.end()) {
            CPUList cpus;
            if (type & Type::PER_THREAD) {
                /* If we have per-thread CPU assignments, get the available CPUs for this thread
                 * from the mapping */
                cpus = active_mapping.cpu(name);
            } else {
                /* Otherwise we allow all CPUs that are used by the mapping */
                cpus = active_mapping.cpus;
            }

            threads.emplace_back(tid, name, cpus);

            /* Instead we assign the thread the CPUs of the mapping and let it migrate around their. */
            logger->info(" * enabled cpu(s) %s\n", string_util::join(cpus.cpulist(num_cpus), ",").c_str());

            if (type & Type::PER_THREAD) {
                /* Inform the client, that it has to move this thread to a specific CPU */
                tetris::ServerMessage msg;

                msg.set_type(tetris::ServerMessage::MOVE_THREADS);

                Connection conn(push_path());
                protobuf_util::Send(conn.locked(), msg);

                tetris::ClientResponse resp;
                protobuf_util::Receive(conn.locked(), resp);
                if (resp.type() != tetris::ClientResponse::ACKNOWLEDGE) {
                    logger->warning(" --> Client responded with an error\n");
                }
            } else {
                cpu_set_t cset = cpus.cpu_set();
                sched_setaffinity(tid, sizeof(cset), &cset);
            }
        } else {
            logger->warning("Duplicate thread '%s' [%i]\n", name.c_str(), tid);
        }
    }

    void delete_thread(int tid)
    {
        auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
        if (it != threads.end()) {
            logger->info("Deleting thread '%s' [%i] from client '%s' [%d]\n", it->name.c_str(), it->tid, exec.c_str(), pid);
            threads.erase(it);
        } else {
            logger->warning("No thread known with ID %i, can't delete\n", tid);
        }
    }

public:
    Client(const Client &) = delete;

    Client(const ConnectionPtr &conn) :
            connection{conn}, exec{}, pid{-1}, threads{},
            mappings{}, active_mapping{}, type{Type::PASSIV}
    {}

    ~Client()
    {
        if (pid != -1)
            logger->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
    }

    std::string push_path() const
    {
        std::stringstream path{};
        path << "/tmp/tetris_push_listener_" << pid;

        return path.str();
    }

    CPUList cpus() const
    {
        return active_mapping.cpus;
    }

    void activate_type(const Type& t)
    {
        type |= t;
    }

    void deactivate_type(const Type& t)
    {
        type &= ~t;
    }

    void update_mapping(const Mapping &new_mapping)
    {
        if (new_mapping.name == active_mapping.name)
            return;

        logger->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid, new_mapping.name.c_str());
        active_mapping = new_mapping;

        for (auto &t : threads) {
            CPUList cpus;
            if (type & Type::PASSIV)
                cpus = active_mapping.cpus;
            else
                cpus = active_mapping.cpu(t.name);

            logger->info(" * remap thread '%s' [%i] from cpu(s) %s to cpu(s) %s\n", t.name.c_str(), t.tid,
                         string_util::join(t.cpus.cpulist(num_cpus), ",").c_str(),
                         string_util::join(cpus.cpulist(num_cpus), ",").c_str());

            t.cpus = cpus;

            cpu_set_t mask = cpus.cpu_set();
            if (sched_setaffinity(t.tid, sizeof(cpu_set_t), &mask) != 0)
                logger->warning("Failed to set cpu affinity for thread '%s': %s\n", t.name.c_str(), strerror(errno));
        }

        logger->info(" * done\n");
    }

    tetris::ServerResponse handle_message(const tetris::ClientMessage &msg)
    {
        tetris::ServerResponse response{};
        response.set_type(tetris::ServerResponse::ERROR);

        switch(msg.type()) {
            case tetris::ClientMessage::MAPPINGS: 
                break;
            case tetris::ClientMessage::OPTIMIZATION_TARGET:
                break;
            case tetris::ClientMessage::FEATURE_SUBSCRIBE:
                break;
            case tetris::ClientMessage::REGISTER_THREAD:
                if (msg.has_thread_info()) {
                    if (msg.thread_info().has_name())
                        new_thread(msg.thread_info().tid(), msg.thread_info().name());
                    else
                        new_thread(msg.thread_info().tid());

                    response.set_type(tetris::ServerResponse::ACKNOWLEDGE);
                }
                break;
            case tetris::ClientMessage::DELETE_THREAD:
                if (msg.has_thread_info()) {
                    delete_thread(msg.thread_info().tid());

                    response.set_type(tetris::ServerResponse::ACKNOWLEDGE);
                }
                break;
        }

        return response;
    }
};


/***
 * Client Manager
 ***/

class Manager
{
private:
    std::map<int, Client> _clients;

    CPUList _blocked_cpus;

public:
    Manager() = default;

    void client_connect(int fd, const ConnectionPtr &conn)
    {
        _clients.emplace(fd, conn);
    }

    void client_disconnect(int fd)
    {
        _clients.erase(fd);
    }

    bool client_message(int fd)
    try
    {
        Client &c = _clients.at(fd);
        ConnectionPtr conn = c.connection;

        bool done = false;
        bool close = false;

        while (!done) {
            if (c.pid == -1) {
                /* The client is not yet fully registered with the server. Until now we only accept
                 * registration requests. */
                tetris::RegistrationRequest request{};
                auto res = protobuf_util::Receive(conn->locked(), request);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    c.pid = request.pid();
                    c.exec = request.exec();

                    logger->info(" -> The client registered! '%s' [%d]\n", c.exec.c_str(), c.pid);

                    /* Construct and send the server's registration response */
                    tetris::RegistrationResponse response{};
                    response.set_id(fd);

                    if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
                        logger->error("Failed to acknowledge the new-thread message\n");
                }
            } else {
                /* The client is fully registered, receive the message and let
                 * the client handle it properly */
                tetris::ClientMessage msg{};
                auto res = protobuf_util::Receive(conn->locked(), msg);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    auto response = c.handle_message(msg);
                    if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
                        logger->error("Failed to acknowledge the new-thread message\n");
                }
            }
        }
        return close;
    } catch (std::out_of_range) {
        logger->warning("Received message for unknown client %i\n", fd);
        return true;
    } catch (std::runtime_error &e) {
        logger->warning("Error working with message for client %i: %s", fd, e.what());
        return true;
    }

    void print_mappings()
    {
        std::cout << "Currently active mappings:" << std::endl
                  << "==========================" << std::endl;
        for (const auto&[name, client] : _clients) {
            std::cout << "Client '" << client.exec << "' [" << client.pid << "] (ID: " << name << ")" << std::endl;
            std::cout << "-> mapping: " << client.active_mapping.name << " ["
                      << client.active_mapping.equivalence_class().name() << "]" << std::endl;

            std::cout << "-> threads:" << std::endl;
            for (const auto &t : client.threads)
                std::cout << "--> " << t.name << "(" << t.tid << "): "
                          << string_util::join(t.cpus.cpulist(num_cpus), ",") << std::endl;
        }
        std::cout << "======= END OF LIST =======" << std::endl;
    }
};


void usage()
{
    std::cout << "usage: tetrisserver [-h]" << std::endl
              << std::endl
              << "Options:" << std::endl
              << "   -h, --help           show this help message." << std::endl
              << std::endl;
}

int main(int argc, char *argv[])
{
    /* Parsing command line arguments. */
    if (argc > 2) {
        usage();
        return 1;
    } else if(argc == 2) {
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

    /* Setting up the server socket */
    Socket server_sock;
    int sock_fd = -1;
    try {
        server_sock.open(SERVER_SOCKET);
        server_sock.non_blocking();
        server_sock.listening();
        sock_fd = server_sock.fd();
    } catch (std::runtime_error &e) {
        std::cerr << "Failed to open socket" << std::endl
                  << e.what() << std::endl;
        return 1;
    }

    logger->info(" * Server socket: %s (%i)\n", server_sock.path(), sock_fd);

    /* Setup signal handling */
    int sig_fd = -1;
    {
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
        sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
        if (sig_fd == -1) {
            std::cerr << "Failed to create signal fd." << std::endl
                      << strerror(errno) << std::endl;
            return 1;
        }
    }

    /* Setup the epoll event loop. */
    int epoll_fd = -1;
    {
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            std::cerr << "Failed to initialize epoll." << std::endl
                      << strerror(errno) << std::endl;
            return 1;
        }

        epoll_event e;
        for (auto fd : {sock_fd, sig_fd}) {
            e.data.fd = fd;
            e.events = EPOLLIN;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
                std::cerr << "Failed to add socket " << fd << " to epoll." << std::endl
                          << strerror(errno) << std::endl;
                return 1;
            }
        }
    }

    /* The event loop */
    epoll_event events[MAXEVENTS];
    bool done = false;

    while (!done) {
        int n;

        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        for (int i = 0; i < n; ++i) {
            epoll_event *cur = &events[i];

            if (cur->data.fd == sock_fd) {
                /* There are a new connections at the server socket.
                 * Connect with all of them. */
                while (1) {
                    sockaddr_un in_sock;
                    socklen_t in_sock_size = sizeof(in_sock);
                    int infd = ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We connected to all possible connections already.
                             * Continue with the main loop. */
                            break;
                        } else {
                            logger->error("An error happened while accepting a connection: %s", strerror(errno));
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
                        logger->error("Failed to add new connection to epoll: %s", strerror(errno));
                        ::close(infd);
                    } else {
                        logger->info("A new client connected (%i)\n", infd);

                        manager.client_connect(infd, in_conn);
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
                            logger->error("An error happened while reading data from signal fd: %s", strerror(errno));

                        break;
                    }

                    logger->info("Received a signal (%i)\n", siginfo.ssi_signo);

                    switch (siginfo.ssi_signo) {
                        case SIGUSR1:
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

    std::cout << "Exiting" << std::endl;
    ::close(sig_fd);

    return 0;
}
