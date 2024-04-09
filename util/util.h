#ifndef __UTIL_H__
#define __UTIL_H__

#pragma once

#include <cstdint>


namespace util {

void make_fd_non_blocking(int fd);

uint64_t ctime_to_ms(uint64_t ctime);

} /* namespace util */

#endif /* __UTIL_H__ */
