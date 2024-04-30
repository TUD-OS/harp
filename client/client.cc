//
// Created by dylan on 06/08/2020.
//

#include "client.h"
#include "concrete_client.h"

namespace tetris {

std::unique_ptr<Client> ClientProvider::_instance;

void ClientProvider::initialize(const std::string &socket_path,
                                const std::string &platform_path,
                                const std::string &mapping_path,
                                bool mapping_coarse_grained) {
  _instance = std::make_unique<ConcreteClient>(
      socket_path, platform_path, mapping_path, mapping_coarse_grained);
}

void ClientProvider::finalize() { _instance.reset(); }

Client *ClientProvider::get_instance() { return _instance.get(); }

} // namespace tetris
