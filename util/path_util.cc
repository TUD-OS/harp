//
// Created by dylan on 07/08/2020.
//

#include "path_util.h"

namespace path_util {

/* Implementations */
std::string abspath(const std::string &path)
{
    return isabs(path) ? path : join(getcwd(), path);
}

std::string basename(const std::string &path)
{
    return split(path).second;
}

std::string dirname(const std::string &path)
{
    return split(path).first;
}

bool exists(const std::string &path)
{
    return ::access(path.c_str(), F_OK) == 0;
}

std::string expanduser(const std::string &path)
{
    if (path.empty()) {
        return path;
    } else {
        if (path[0] == '~') {
            return std::string{getpwuid(getuid())->pw_dir} + path.substr(1);
        } else {
            return path;
        }
    }
}

std::string extension(const std::string &path)
{
    return splitext(path).second;
}

std::string filename(const std::string &path)
{
    return splitext(split(path).second).first;
}

void for_each_file(const std::string &path, const std::function<void(const std::string &)> &cb)
{
    auto dir = opendir(path.c_str());
    if (dir == nullptr) {
        throw std::runtime_error{"Failed to open directory at " + path};
    }

    dirent *cur;
    while ((cur = readdir(dir)) != nullptr) {
        if (cur->d_type == DT_REG || cur->d_type == DT_LNK || cur->d_type == DT_UNKNOWN) {
            std::string file_name{cur->d_name};

            cb(join(path, file_name));
        }
    }
}

void for_each_folder(const std::string &path, const std::function<void(const std::string &)> &cb) {
    auto dir = opendir(path.c_str());
    if (dir == nullptr) {
        throw std::runtime_error{"Failed to open directory at " + path};
    }

    dirent *cur;
    while ((cur = readdir(dir)) != nullptr) {
        std::string name{cur->d_name};

        if ((cur->d_type == DT_DIR) && (*cur->d_name != '.')) {
            cb(join(path, name));
        } else if (cur->d_type == DT_LNK) {
            std::string symbolic_link_path{join(path, name)};

            // Test if the symbolic link refers to a directory.
            auto symbolic_dir = opendir(symbolic_link_path.c_str());
            if (symbolic_dir != nullptr) {
                closedir(symbolic_dir);
                // Call the callback on the directory path.
                cb(symbolic_link_path);
            }
        }
    }
}

std::string getcwd() {
    char cwd[512];
    ::getcwd(cwd, sizeof(cwd));

    return std::string{cwd};
}

bool isabs(const std::string &path)
{
    if (path.empty())
        return false;
    return path[0] == '/';
}

std::string join(const std::string &first, const std::string &second, char delim)
{
    return first + delim + second;
}

std::pair<std::string, std::string> split(const std::string &path, char delim)
{
    size_t dpos = std::string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == std::string::npos) {
            return make_pair("", path);
        } else if (dpos != 0) {
            if (path[dpos - 1] == '\\')
                continue;

            return make_pair(path.substr(0, dpos), path.substr(dpos + 1));
        } else {
            return make_pair("/", path.substr(1));
        }
    }

    return make_pair(path, "");
}

std::pair<std::string, std::string> splitext(const std::string &path, char delim)
{
    size_t dpos = std::string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == std::string::npos) {
            return make_pair(path, "");
        } else if (dpos != 0) {
            if (path[dpos - 1] == '\\')
                continue;

            return make_pair(path.substr(0, dpos), path.substr(dpos));
        } else if (dpos == 0) {
            return make_pair(path, "");
        }
    }

    return make_pair(path, "");
}

}