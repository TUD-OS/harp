//
// Created by dylan on 07/08/2020.
//

#include "path_util.h"

using namespace std;

/* Implementations */
string path_util::abspath(const string &path) {
    return isabs(path) ? path : join(getcwd(), path);
}

string path_util::basename(const string &path) {
    return split(path).second;
}

string path_util::dirname(const string &path) {
    return split(path).first;
}

bool path_util::exists(const string &path) {
    return ::access(path.c_str(), F_OK) == 0;
}

string path_util::expanduser(const string &path) {
    if (path.empty()) {
        return path;
    } else {
        if (path[0] == '~') {
            return string{getpwuid(getuid())->pw_dir} + path.substr(1);
        } else {
            return path;
        }
    }
}

string path_util::extension(const string &path) {
    return splitext(path).second;
}

string path_util::filename(const string &path) {
    return splitext(split(path).second).first;
}

void path_util::for_each_file(const string &path, const function<void(const string &)> &cb) {
    auto dir = opendir(path.c_str());
    if (dir == nullptr) {
        throw runtime_error{"Failed to open directory at " + path};
    }

    dirent *cur;
    while ((cur = readdir(dir)) != nullptr) {
        if (cur->d_type == DT_REG || cur->d_type == DT_LNK || cur->d_type == DT_UNKNOWN) {
            string file_name{cur->d_name};

            cb(join(path, file_name));
        }
    }
}

string path_util::getcwd() {
    char cwd[512];
    ::getcwd(cwd, sizeof(cwd));

    return string{cwd};
}

bool path_util::isabs(const string &path) {
    if (path.empty())
        return false;
    return path[0] == '/';
}

string path_util::join(const string &first, const string &second, char delim) {
    return first + delim + second;
}

pair<string, string> path_util::split(const string &path, char delim) {
    size_t dpos = string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == string::npos) {
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

pair<string, string> path_util::splitext(const string &path, char delim) {
    size_t dpos = string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == string::npos) {
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