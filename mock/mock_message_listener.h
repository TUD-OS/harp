#ifndef __MOCK_MESSAGE_LISTENER_H__
#define __MOCK_MESSAGE_LISTENER_H__

#include <map>
#include "client/client.h"
#include "util/socket.h"

namespace tetris {

/* Forward declare MockClient */
class MockClient;

/**
 * \brief Moch Message Listener.
 *
 * A mock message listener runs in a TETRiS mock client instance in order to receive request from the TETRiS mock server.
 * It handles the logic responsible for the redistribution of message to TETRiS features.
 */
class MockMessageListener {
public:
    explicit MockMessageListener(const std::string &socket_path, MockClient *client);

    /**
     * \brief Destroys the push message listener, closing the socket and joining the listener thread.
     */
    ~MockMessageListener();

    /**
     * \brief Adds a TETRiS feature with its id to the subscribers map.
     * \param feature_id Feature ID.
     * \param feature Pointer to feature.
     */
    void add_subscriber(const FeatureID &feature_id, Feature *feature);

    /**
     * \brief Forwards the TETRiS server message to the specified feature.
     * \param msg Message to forward.
     * \return Forwarded response.
     */
    ClientResponse forward(const MockServerMessage &msg) const;

private:
    /**
     * \brief Callback function call in listening thread.
     * \return None.
     */
    static void *listening(void *args);

    /// \brief Push message listening socket.
    Socket _listening_socket;

    /// \brief Pointer to the handling client
    MockClient *_client;

    /// \brief Listener thread id.
    pthread_t _listener_thread;

    /// \brief Subscribers.
    std::map<FeatureID, Feature *> _subscribers;
};

} /* namespace tetris */

#endif //__MOCK_PUSH_MESSAGE_LISTENER_H__
