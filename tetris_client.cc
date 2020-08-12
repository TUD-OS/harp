//
// Created by dylan on 06/08/2020.
//

#include "tetris_client.h"
#include "concrete_client.h"

std::unique_ptr<tetris::Client> tetris::ClientProvider::_instance;

void tetris::ClientProvider::initialize(const std::string &socket_path)
{
    _instance = std::make_unique<tetris::ConcreteClient>(socket_path);
}

void tetris::ClientProvider::finalize()
{ _instance.reset(); }

tetris::Client *tetris::ClientProvider::get_instance()
{ return _instance.get(); }
