#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#pragma once


#include "lock_util.h"
#include "path_util.h"
#include "tetris.h"
#include "debug_util.h"
#include "util.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


class Connection : public Lockable<Connection>
{
   public:
    enum class InState {
        MORE = 1,
        DONE = 2,
        CLOSED = 3,
        AGAIN = 4
    };

    enum class OutState {
        DONE = 1,
        RETRY = 2,
    };

   private:
    int             _fd;
    sockaddr_un     _sock;
    bool            _blocking;

    /* Buffers used for incomplete read operations */
    ssize_t         _read_d;
    ssize_t         _size_d;
    char*           _data;

    void close()
    {
        if (_fd == -1)
            return;

        ::close(_fd);
        _fd = -1;
    }

   public:
    Connection() :
        _fd{-1}, _sock{}, _blocking{true}, _read_d{0}, _size_d{0}, _data{nullptr}
    {}

    explicit Connection(const std::string& sock_path) :
        _fd{-1}, _sock{}, _blocking{true}, _read_d{0}, _size_d{0}, _data{nullptr}
    {
        connect(sock_path);
    }

    Connection(int fd, const sockaddr_un& sock, bool blocking=true) :
        _fd{fd}, _sock{sock}, _blocking{blocking}, _read_d{0}, _size_d{0}, _data{nullptr}
    {}

    Connection(const Connection&) = delete;
    Connection(Connection&& o) :
        _fd{o._fd}, _sock{o._sock}, _blocking{o._blocking}, _read_d{o._read_d},
        _size_d{o._size_d}, _data{o._data}
    {
        o._fd = -1;
        o._read_d = 0;
        o._size_d = 0;
        o._data = nullptr;
    }

    ~Connection()
    {
        if (_read_d) {
            free(_data);
            _read_d = 0;
            _size_d = 0;
            _data = nullptr;
        }

        close();
    }

    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&& o)
    {
        close();

        _fd = o._fd;
        _sock = o._sock;
        _blocking = o._blocking;
        _read_d = o._read_d;
        _size_d = o._size_d;
        _data = o._data;

        o._fd = -1;
        o._read_d = 0;
        o._size_d = 0;
        o._data = nullptr;

        return *this;
    }

    int fd() const
    {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        return _fd;
    }

    const char* path() const
    {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        return _sock.sun_path;
    }

    void connect(const std::string& sock_path)
    {
        if (_fd != -1) {
            throw std::runtime_error{"The connection is already initialized."};
        }

        /* Check if the socket already exists. */
        if (!path_util::exists(sock_path)) {
            throw std::runtime_error{"The specified socket file does not exist."};
        }

        /* Create the socket. */
        if ((_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error{"Failed to acquire socket fd."};
        }

        _sock.sun_family = AF_UNIX;
        std::strcpy(_sock.sun_path, sock_path.c_str());
        if (::connect(_fd, reinterpret_cast<sockaddr*>(&_sock), sizeof(_sock)) == -1) {
            ::close(_fd);
            _fd = -1;
            throw std::runtime_error{"Failed to connect to socket."};
        }
    }

    void non_blocking()
    {
        if (_fd == -1) {
            throw std::runtime_error{"Socket not initialized."};
        }

        if (_blocking) {
            util::make_fd_non_blocking(_fd);
            _blocking = false;
        }
    }

    template<typename T>
    InState read(T& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        if (_read_d == 0) {
            _data = static_cast<char*>(malloc(sizeof(data)));
            _size_d = sizeof(data);
        } else {
            LOGGER->debug("Continuing incomplete message %d/%d (%d missing)\n", _read_d, _size_d, _size_d-_read_d);
        }

        do {
            ssize_t size = ::read(_fd, _data+_read_d, _size_d-_read_d);
            if (size == -1) {
                if (errno == EAGAIN && !_blocking) {
                    if (_read_d != 0) {
                        LOGGER->debug("Incomplete message %d/%d read\n", _read_d, _size_d);
                        return InState::AGAIN;
                    } else
                        return InState::DONE;
                }

                throw std::runtime_error{"Read failed."};
            } else if (size == 0) {
                return InState::CLOSED;
            }

            _read_d += size;
        } while (_read_d < _size_d);

        memcpy(&data, _data, _size_d);
        free(_data);
        _read_d = 0;
        _size_d = 0;
        _data = nullptr;

        return _blocking ? InState::DONE : InState::MORE;
    }

    InState read(std::vector<uint8_t>& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        if (_read_d == 0) {
             // Read the vector size through the socket.
            uint32_t vector_size = 0;
            auto result = ::read(_fd, &vector_size, sizeof(vector_size));
            if (result == -1) {
                if (!_blocking) {
                    if (errno == EAGAIN)
                        return InState::AGAIN;
                    else
                        return InState::MORE;
                }

                throw std::runtime_error("Read failed.");
            } else if (result == 0) {
                return InState::CLOSED;
            }

            _data = static_cast<char*>(malloc(sizeof(vector_size)));
            _size_d = vector_size;
        } else {
            LOGGER->debug("Continuing incomplete message %d/%d (%d missing)\n", _read_d, _size_d, _size_d-_read_d);
        }

        // Read the vector through the socket.
        do {
            ssize_t size = ::read(_fd, _data+_read_d, _size_d-_read_d);
            if (size == -1) {
                if (errno == EAGAIN && !_blocking) {
                    if (_read_d != 0) {
                        LOGGER->debug("Incomplete message %d/%d read\n", _read_d, _size_d);
                        return InState::AGAIN;
                    } else
                        return InState::MORE;
                }

                throw std::runtime_error{"Read failed."};
            } else if (size == 0) {
                return InState::CLOSED;
            }

            _read_d += size;
        } while (_read_d < _size_d);

        /* Copy the content from our intermediate buffer over to the actual buffer */
        data.clear();
        data.insert(data.begin(), _data, _data+_read_d);
        free(_data);
        _read_d = 0;
        _size_d = 0;
        _data = nullptr;

        return _blocking ? InState::DONE : InState::MORE;
    }

    template<typename T>
    OutState write(const T& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        ssize_t size = ::write(_fd, &data, sizeof(data));
        if (size == -1) {
            if (errno == EAGAIN && !_blocking)
                return OutState::RETRY;
            throw std::runtime_error{"Write failed."};
        } else if (size != sizeof(data)) {
            throw std::runtime_error{"Failed to write complete data!"};
        }

        return OutState::DONE;
    }

    OutState write(const std::vector<uint8_t>& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }
        // Write the size of the vector before sending it.
        uint32_t vector_size = data.size();
        OutState write_state = write(vector_size);
        if (write_state == OutState::RETRY)
            return OutState::RETRY;
        // Write the data contained in the vector.
        ssize_t size = ::write(_fd, data.data(), data.size());
        if (size == -1) {
            if (errno == EAGAIN && !_blocking)
                return OutState::RETRY;
            throw std::runtime_error{"Write failed."};
        } else if (size != data.size()) {
            throw std::runtime_error{"Failed to write complete data!"};
        }

        return OutState::DONE;
    }

};

using LockedConnection = Locked<Connection>;

#endif /* __CONNECTION_H__ */
