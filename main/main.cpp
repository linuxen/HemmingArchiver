#include <exception>
#include <iostream>

#include "archive.h"
#include "parser.h"

int main(int argc, char* argv[]) {
    try {
        Options opt = ParseCommandLine(argc, argv);

        int actionCount = 0;
        actionCount += opt.Create ? 1 : 0;
        actionCount += opt.extract ? 1 : 0;
        actionCount += opt.List ? 1 : 0;
        actionCount += opt.Append ? 1 : 0;
        actionCount += opt.Delete ? 1 : 0;
        actionCount += opt.Concatenate ? 1 : 0;

        if (actionCount == 0) {
            std::cerr << "No action specified. Use --create, --extract, --list, --append, --delete or --concatenate" << '\n';
            return 1;
        }
        if (actionCount > 1) {
            std::cerr << "Only one action can be specified" << '\n';
            return 1;
        }
        if (opt.archiveName.empty()) {
            std::cerr << "Archive name is required (-f or --file)" << '\n';
            return 1;
        }

        Archive arch(opt.archiveName);

        if (opt.Create) {
            if (opt.fileCount == 0) {
                std::cerr << "No input files for create" << '\n';
                return 1;
            }
            arch.Create(opt.files, opt.fileCount);
        }
        else if (opt.extract) {
            if (opt.fileCount == 0) {
                arch.ExtractAll();
            }
            else {
                arch.Extract(opt.files, opt.fileCount);
            }
        }
        else if (opt.List) {
            arch.List();
        }
        else if (opt.Append) {
            if (opt.fileCount == 0) {
                std::cerr << "No input files for append" << '\n';
                return 1;
            }
            arch.Append(opt.files, opt.fileCount);
        }
        else if (opt.Delete) {
            if (opt.fileCount == 0) {
                std::cerr << "No files for delete" << '\n';
                return 1;
            }
            arch.Delete(opt.files, opt.fileCount);
        }
        else if (opt.Concatenate) {
            if (opt.fileCount < 2) {
                std::cerr << "Concatenate requires at least two input archives" << '\n';
                return 1;
            }
            arch.Concatenate(opt.files, opt.fileCount);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
