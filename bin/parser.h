#pragma once

#include <iostream>
#include <string>

struct Options {
    bool Create = false;
    bool extract = false;
    bool List = false;
    bool Append = false;
    bool Delete = false;
    bool Concatenate = false;

    std::string archiveName;

    static constexpr int maxfiles = 256;
    std::string files[maxfiles];
    int fileCount = 0;
};

Options ParseCommandLine(int argc, char* argv[]);
