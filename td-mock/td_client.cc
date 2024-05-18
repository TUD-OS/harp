#include "client/client.h"
#include "util/debug_util.h"
#include "util/connection.h"
#include "util/protobuf_util.h"
#include "proto/tetris.pb.h"

#include <dlfcn.h>
#include <link.h>

using namespace tetris;

class TDClient : public Client {
   public:
    explicit TDClient(const std::string &server_socket_path) {
        _tetris_server_connection.connect(server_socket_path);
        RegistrationRequest request{};

        request.set_pid(getpid());
        char exec[512];
        memset(exec, 0, sizeof(exec));
        readlink("/proc/self/exe", exec, sizeof(exec));
        request.set_exec(exec);
        request.set_mapping_type(RegistrationRequest::COARSE_GRAINED);

        // Send the command.
        try {
            RegistrationResponse response{};
            protobuf_util::Send(_tetris_server_connection.locked(), request);
            protobuf_util::Receive(_tetris_server_connection.locked(), response);

            LOGGER->info("TETRIS-ID: %d\n", response.id());
        } catch (std::exception &e) {
            LOGGER->info("NO TETRIS-TD Support\n");
        }
    }

    /*
     * \copydoc bind(TETRiS::Feature *feature)
     */
    void bind(tetris::Feature *feature) override {}

    /**
     * \copydoc bind(TETRiS::MappingFeature *feature)
     */
    void bind(tetris::MappingFeature *feature) override {}

    /**
     * \copydoc send(const ClientMessage &msg)
     */
    ServerResponse send(const ClientMessage &msg) override {
        return {};
    }

    ClientResponse handle(const ServerMessage &msg) override {
        return {};
    }

   private:
    Connection _tetris_server_connection;
};

TDClient *client;

extern "C"
void __attribute__((constructor)) setup(void)
{
    client = new TDClient(SERVER_SOCKET);
}
