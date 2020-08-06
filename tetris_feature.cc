//
// Created by dylan on 06/08/2020.
//

#include "tetris_feature.h"

TETRiS::Feature::Feature() : _client(nullptr) {}

void TETRiS::Feature::accept(TETRiS::Client *client) { _client = client; }

bool TETRiS::Feature::is_bound() const { return _client != nullptr; }

TETRiS::Client *TETRiS::Feature::get_client() const { return _client; }
