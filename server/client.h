#ifndef __CLIENT_H__
#define __CLIENT_H__

#pragma once

#include <optional>

#include "proto/tetris.pb.h"

#include "util/connection.h"
#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/operating_point_table.h"
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
  };

public:
  ConnectionPtr connection;
  std::string exec;
  int pid;

  std::unique_ptr<tetris::OperatingPointTable> op_table;
  std::optional<tetris::OperatingPointAllocation> active_op;

  int type;

private:
  Manager &_manager;

  bool receive_ops(const tetris::ClientMessage::OperatingPointsInfo &);

public:
  Client(const Client &) = delete;

  Client(const ConnectionPtr &conn, Manager &manager);

  ~Client() {
    if (pid != -1)
      LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
  }

  std::string push_path() const;

  tetris::CPUThreadSet threads() const {
    if (active_op.has_value()) {
      return active_op->threads();
    }
    return tetris::CPUThreadSet{};
  }

  void activate_type(const Type &t) { type |= t; }

  void deactivate_type(const Type &t) { type &= ~t; }

  void activate_op(const tetris::OperatingPointAllocation &new_op);

  tetris::ServerResponse handle_message(const tetris::ClientMessage &msg);
};

#endif /* __CLIENT_H__ */
