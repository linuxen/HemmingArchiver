#pragma once

#include <cstdint>
#include <iostream>
#include <string>

struct FileEntry {
    std::string name;
    uint64_t originalSize;
};

class Archive {
public:
    explicit Archive(std::string path);

    void Create(const std::string files[], int fileCount);
    void Extract(const std::string files[], int fileCount);
    void ExtractAll();
    void List();
    void Append(const std::string files[], int fileCount);
    void Delete(const std::string files[], int fileCount);
    void Concatenate(const std::string archives[], int archiveCount);

private:
    std::string path_;
    uint64_t GetFileSize(const std::string& path);
};
