#ifndef __CLIENT_H__
#define __CLIENT_H__

#pragma once

#include "proto/tetris.pb.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"
#include "util/protobuf_util.h"

#include <optional>

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
  };

public:
  ConnectionPtr connection;
  std::string exec;
  int pid;

  std::vector<tetris::OperatingPoint> ops;
  std::optional<tetris::OperatingPointAllocation> active_op;

  double progress; // current progress (0.0...1.0)
  std::chrono::high_resolution_clock::time_point
      progress_tp; // last progress update

  int type;

private:
  Manager *_manager;

  bool receive_ops(const tetris::ClientMessage::OperatingPointsInfo &);

public:
  Client(const Client &) = delete;

  Client(const ConnectionPtr &conn, Manager *manager);

  ~Client() {
    if (pid != -1)
      LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
  }

  std::string push_path() const;

  tetris::CPUThreadSet cpus() const {
    if (active_op.has_value()) {
      return active_op->base.cpus;
    }
    return tetris::CPUThreadSet{};
  }

  void activate_type(const Type &t) { type |= t; }

  void deactivate_type(const Type &t) { type &= ~t; }

  void activate_op(const tetris::OperatingPointAllocation &new_op);

  tetris::ServerResponse handle_message(const tetris::ClientMessage &msg);

  void update_progress(std::chrono::high_resolution_clock::time_point new_tp,
                       bool reset = false) {
    if (active_op.has_value()) {
      std::chrono::duration<double, std::milli> diff = new_tp - progress_tp;
      auto diff_ms = diff.count();
      auto extime = active_op->characteristic("execution_time");
      auto cur_progress = diff_ms / extime;
      progress += cur_progress;

      if (progress >= 1.0 && reset) {
        LOGGER->info("Progress of the client '%s' [%i] is beyound 1.0, "
                     "resetting to 0.9\n",
                     exec.c_str(), pid);
        progress = 0.9;
      }
    }
    progress_tp = new_tp;
  }
};

#endif /* __CLIENT_H__ */
