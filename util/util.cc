//
// Created by dylan on 07/08/2020.
//

#include "util.h"

#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>


void util::make_fd_non_blocking(int fd)
{
    int flags;

    flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error{"Failed to get flags of the socket."};
    } else {
        flags |= O_NONBLOCK;
        if (::fcntl(fd, F_SETFL, flags) == -1) {
            throw std::runtime_error{"Failed to set flags on socket."};
        }
    }
}


uint64_t util::ctime_to_ms(uint64_t ctime)
{
    static auto clk_tck = sysconf(_SC_CLK_TCK);

    return (ctime * 1000) / clk_tck;
}
