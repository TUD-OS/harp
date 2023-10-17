#ifndef __CLIENT_H__
#define __CLIENT_H__

#pragma once

#include "filter.h"
#include "proto/tetris.pb.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"
#include "util/protobuf_util.h"

using ConnectionPtr = std::shared_ptr<Connection>;

class Manager;

/**
 * \class Client
 * \brief Represents an application client in the manager.
 *
 * This class manages client's active CPU mapping and dynamically updates
 * mapping regions.
 */
class Client {
 public:
  /* The management type of an application */
  enum Type : int {
    /* PASSIV: the default type --> The application will be moved around as a
     * whole with no additional management. Mappings and characteristics
     * are not required for this type of application */
    PASSIV = 0x1,

    /* ACTIVE: the default managed type --> TETRiS will actively manage the
     * application. For this type a simple mapping has to be provided. TETRiS
     * will try to optimize the application depending on the application's
     * criteria and the overall system state */
    ACTIVE = 0x2,

    /* PER_THREAD: a subtype of ACTIVE --> TETRiS will distinguish mappings that
     * have different assignments of threads to CPUs as different mappings and
     * choose them according to the application's otimization criteria. For this
     * type an appropriate mapping has to be provided by the client. */
    PER_THREAD = 0x4,

    /* SCALABLE: a subtype of ACTIVE --> TETRiS will send scaling information to
     * the application. For this type an appropriate mapping has to be provided
     * by the client */
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

    bool operator()(const tetris::OperatingPoint &other, const tetris::OperatingPoint &best) {
      return _comp(other.characteristic(_criteria),
                   best.characteristic(_criteria));
    }

    bool operator()(const tetris::OperatingPointAllocation &other, const tetris::OperatingPointAllocation &best) {
      return _comp(other.base.characteristic(_criteria), 
                   best.base.characteristic(_criteria));
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

  std::vector<tetris::OperatingPoint> ops;
  tetris::OperatingPointAllocation active_op;

  std::string push_listener_path;

  int type;

  Filter filter;
  Comp comp;

private:
  bool receive_ops(const tetris::ClientMessage::OperatingPointsInfo&);

public:
    Client(const Client &) = delete;

    Client(const ConnectionPtr &conn);

    ~Client()
    {
        if (pid != -1)
            LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
    }

    std::string push_path() const
    {
        return push_listener_path;
    }

    tetris::CPUThreadSet cpus() const
    {
        return active_op.base.cpus;
    }

    void activate_type(const Type& t)
    {
        type |= t;
    }

    void deactivate_type(const Type& t)
    {
        type &= ~t;
    }

    void activate_op(const tetris::OperatingPointAllocation &new_op);

    tetris::ServerResponse handle_message(const tetris::ClientMessage &msg);
};

#endif /* __CLIENT_H__ */
