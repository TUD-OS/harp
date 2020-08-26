#ifndef __UTIL_H__
#define __UTIL_H__

#pragma once


#include <stdexcept>

#include <fcntl.h>


namespace util {

void make_fd_non_blocking(int fd);

} /* namespace util */

#endif /* __UTIL_H__ */
