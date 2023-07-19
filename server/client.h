#ifndef __CLIENT_H__
#define __CLIENT_H__

#pragma once

#include "filter.h"
#include "proto/tetris.pb.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "util/protobuf_util.h"
#include "util/mapping.h"

using ConnectionPtr = std::shared_ptr<Connection>;

/**
 * \class Client
 * \brief Represents an application client in the manager.
 *
 * This class manages client's active CPU mapping and dynamically updates
 * mapping regions.
 */
class Client {
 public:
  /**
   * \struct Thread
   * \brief Represents a thread within a client, which includes its name, thread
   * id and CPU affinity.
   */
  struct Thread {
    bool named;
    std::string name;
    int tid;
    CPUList cpus;

    Thread(int tid, CPUList cpus)
        : named{false}, name{}, tid{tid}, cpus{cpus}
    {
      std::stringstream ss;
      ss << "thread-" << tid;
      name = ss.str();
    }

    Thread(int tid, const std::string &name, CPUList cpus)
        : name{name}, tid{tid}, cpus{cpus} {}
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

  /**
   * \class Comp
   * \brief Helper class for comparing mappings based on certain criteria.
   */
  class Comp {
   private:
    std::string _criteria;
    bool _more_is_better;
    std::function<bool(const double, const double)> _comp;

   public:
    Comp(const std::string compare_criteria, bool compare_more_is_better)
        : _criteria{compare_criteria}, _more_is_better{compare_more_is_better} {
      if (_more_is_better)
        _comp = std::greater<double>{};
      else
        _comp = std::less<double>{};
    }

    Comp() : _criteria{}, _comp{std::less<double>()} {}

    bool operator()(const Mapping &other, const Mapping &best) {
      return _comp(other.characteristic(_criteria),
                   best.characteristic(_criteria));
    }

    std::string criteria() const { return _criteria; }

    std::string repr() const {
      std::stringstream ss;
      ss << _criteria << "(" << (_more_is_better ? ">" : "<") << ")";

      return ss.str();
    }
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

  Filter filter;
  Comp comp;

private:
    /* Internal interface */
    void new_thread(int tid);
    void new_thread(int tid, const std::string &name);

    void delete_thread(int tid);

public:
    Client(const Client &) = delete;

    Client(const ConnectionPtr &conn) :
            connection{conn}, exec{}, pid{-1}, threads{},
            mappings{}, active_mapping{}, type{Type::PASSIV},
            filter{}, comp{}
    {
        std::stringstream path{};
        path << "/tmp/tetris_push_listener_" << pid;

        push_listener_path = path.str();
    }

    ~Client()
    {
        if (pid != -1)
            LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
    }

    std::string push_path() const
    {
        return push_listener_path;
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

    void update_mapping(const Mapping &new_mapping);

    tetris::ServerResponse handle_message(const tetris::ClientMessage &msg);
};

#endif /* __CLIENT_H__ */
