//
// Created by dylan on 06/08/2020.
//

#include "feature.h"

tetris::Feature::Feature() : _client(nullptr)
{}

void tetris::Feature::accept(tetris::Client *client)
{ _client = client; }

bool tetris::Feature::is_bounded() const
{ return _client != nullptr; }

tetris::Client *tetris::Feature::get_client() const
{ return _client; }
