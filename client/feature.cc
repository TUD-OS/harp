//
// Created by dylan on 06/08/2020.
//

#include "feature.h"

namespace tetris {

Feature::Feature() : _client(nullptr)
{}

void Feature::accept(Client *client)
{ _client = client; }

bool Feature::is_bounded() const
{ return _client != nullptr; }

Client *Feature::get_client() const
{ return _client; }

}