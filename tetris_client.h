//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

namespace TETRiS {
    class Feature;
    /**
     * \brief TETRiS Client API.
     */
    class Client {
    public:
        /* Initializes the push server and other implementation details */
        void initialize();

        /* Destroys the push server. */
        void finalize();

        /* Binds a specific TETRiS feature. During this call, a handshake
         * is sent to the TETRiS server by calling TETRiS::Feature::Handshake() */
        void bind(TETRiS::Feature *feature);

        /* Sends a ClientRequest and returns a ClientResponse. */
        ClientResponse send(const ClientRequest &msg);
    };
}

#endif // __TETRIS_CLIENT_H__
