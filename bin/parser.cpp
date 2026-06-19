#include "parser.h"

#include <stdexcept>
#include <string>

Options ParseCommandLine(int argc, char* argv[]) {
    Options opt;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--create") {
            opt.Create = true;
        }
        else if (arg == "-x" || arg == "--extract") {
            opt.extract = true;
        }
        else if (arg == "-l" || arg == "--list") {
            opt.List = true;
        }
        else if (arg == "-a" || arg == "--append") {
            opt.Append = true;
        }
        else if (arg == "-d" || arg == "--delete") {
            opt.Delete = true;
        }
        else if (arg == "-A" || arg == "--concatenate") {
            opt.Concatenate = true;
        }
        else if (arg.rfind("--file=", 0) == 0) {
            opt.archiveName = arg.substr(7);
        }
        else if (arg.rfind("-f=", 0) == 0) {
            opt.archiveName = arg.substr(3);
        }
        else if (arg == "--file" || arg == "-f") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing archive name after --file/-f");
            }
            opt.archiveName = argv[++i];
        }
        else {
            if (opt.fileCount >= Options::maxfiles) {
                throw std::runtime_error("Too many files");
            }
            opt.files[opt.fileCount++] = arg;
        }
    }

    return opt;
}
