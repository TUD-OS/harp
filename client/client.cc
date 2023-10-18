//
// Created by dylan on 06/08/2020.
//

#include "client.h"
#include "concrete_client.h"

namespace tetris {

std::unique_ptr<Client> ClientProvider::_instance;

void ClientProvider::initialize(const std::string &socket_path, const std::string &platform_path,
        const std::string &mapping_path)
{
    _instance = std::make_unique<ConcreteClient>(socket_path, platform_path, mapping_path);
}

void ClientProvider::finalize()
{ _instance.reset(); }

Client *ClientProvider::get_instance()
{ return _instance.get(); }

}
