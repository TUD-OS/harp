//
// Created by dylan on 06/08/2020.
//

#include "tetris_client.h"
#include "concrete_client.h"

std::unique_ptr<TETRiS::Client> TETRiS::ClientProvider::_instance;

void TETRiS::ClientProvider::initialize(const std::string &socket_path)
{
    _instance = std::make_unique<TETRiS::ConcreteClient>(socket_path);
}

void TETRiS::ClientProvider::finalize()
{ _instance.reset(); }

TETRiS::Client *TETRiS::ClientProvider::get_instance()
{ return _instance.get(); }
