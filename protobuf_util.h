//
// Created by dylan on 30/07/2020.
//

#ifndef DPPM_PROTOBUFHELPER_H
#define DPPM_PROTOBUFHELPER_H

#include "connection.h"

#include <unistd.h>

#include <vector>
#include <cstdint>
#include <stdexcept>

/**
 * \brief The protobuf_util class provides static functions to ease the
 * transmission of protobuf objects through a socket connection.
 */
class protobuf_util {
public:
    /**
     * \brief Receives a protobuf object from a socket connection.
     *
     * \tparam T Type of protobuf object to receive.
     * \param [in] connection Socket connection to read from.
     * \return Object received.
     */
    template<class T>
    static Connection::InState Receive(LockedConnection connection, T &msg) {
        std::vector<uint8_t> raw_data;
        // Waiting to receive the message.
        Connection::InState in_state = connection->read(raw_data);
        // Parse the message from the vector if we succeed to read from the socket.
        if (in_state != Connection::InState::CLOSED)
            msg.ParseFromArray(raw_data.data(), raw_data.size());
        return in_state;
    }

    /**
     * \brief Sends a protobuf object to a socket connection.
     *
     * \tparam T Type of protobuf object to send.
     * \param [in] connection Socket connection to read from.
     * \param [in] msg Protobuf object to send.
     */
    template<class T>
    static Connection::OutState Send(LockedConnection connection, const T &msg) {
        size_t size_msg = msg.ByteSizeLong();
        // Prepare the raw data vector.
        std::vector<uint8_t> raw_data(size_msg);
        msg.SerializeToArray(raw_data.data(), size_msg);
        // Write the vector.
        return connection->write(raw_data);
    }
};

#endif  // DPPM_PROTOBUFHELPER_H
