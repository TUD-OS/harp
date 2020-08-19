#ifndef __PATH_UTIL_H__
#define __PATH_UTIL_H__

#pragma once


#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include <dirent.h>
#include <pwd.h>
#include <unistd.h>


namespace path_util {

/* Prototypes */
std::string abspath(const std::string&);
std::string basename(const std::string&);
std::string dirname(const std::string&);
bool exists(const std::string&);
std::string extension(const std::string&);
std::string expanduser(const std::string&);
std::string filename(const std::string&);
void for_each_file(const std::string&, const std::function<void(const std::string&)>&);
std::string getcwd();
bool isabs(const std::string&);
std::string join(const std::string&, const std::string&, char delim='/');
std::pair<std::string, std::string> split(const std::string&, char delim='/');
std::pair<std::string, std::string> splitext(const std::string&, char delim='.');

} /* namespace path_util */

#endif /* __PATH_UTIL_H__ */
