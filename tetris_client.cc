//
// Created by dylan on 06/08/2020.
//

#include "tetris_client.h"

#include <memory>

void TETRiS::Client::initialize() {
    _instance = std::make_unique<Client>(Client{});
}

void TETRiS::Client::finalize() {
    _instance.reset();
}

TETRiS::Client *TETRiS::Client::get_instance() {
    return _instance.get();
}

void TETRiS::Client::bind(TETRiS::Feature *feature) {

}

TETRiS::ClientResponse TETRiS::Client::send(const TETRiS::ClientRequest &msg) {
    return TETRiS::ClientResponse();
}
