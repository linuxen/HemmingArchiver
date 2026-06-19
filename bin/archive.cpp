#include "archive.h"
#include "codec.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char kHemming[4] = {'H', 'A', 'F', '1'};

struct StoredEntry {
    std::string name;
    uint64_t originalSize = 0;
    uint64_t dataOffset = 0;
};

struct SourceEntry {
    std::string name;
    uint64_t originalSize = 0;
    uint64_t dataOffset = 0;
    std::size_t sourceArchive = 0;
};

template <typename T>
void Write(std::ostream& out, const T& v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(T));
    if (!out) {
        throw std::runtime_error("write error");
    }
}

template <typename T>
void Read(std::istream& in, T& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(T));
    if (!in) {
        throw std::runtime_error("read error");
    }
}

uint64_t EncodedSize(uint64_t originalSize) {
    return originalSize * 2;
}

std::string ArchiveNameFromPath(const std::string& path) {
    fs::path p(path);
    return p.filename().string();
}

std::vector<StoredEntry> ReadArchiveIndex(std::istream& in) {
    char hamming[4];
    in.read(hamming, 4);
    if (in.gcount() != 4 || std::memcmp(hamming, kHemming, 4) != 0) {
        throw std::runtime_error("Bad archive");
    }

    uint32_t fileCount = 0;
    Read(in, fileCount);

    std::vector<StoredEntry> entries;
    entries.reserve(fileCount);

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t nameLen = 0;
        Read(in, nameLen);

        std::string name(nameLen, '\0');
        if (nameLen > 0) {
            in.read(name.data(), nameLen);
            if (!in) {
                throw std::runtime_error("read error");
            }
        }

        uint64_t size = 0;
        Read(in, size);

        entries.push_back({name, size, 0});
    }

    auto position = in.tellg();
    if (position < 0) {
        throw std::runtime_error("read error");
    }

    uint64_t dataOffset = static_cast<uint64_t>(position);
    for (auto& entry : entries) {
        entry.dataOffset = dataOffset;
        dataOffset += EncodedSize(entry.originalSize);
    }

    return entries;
}

void WriteArchiveHeader(std::ostream& out, const std::vector<FileEntry>& entries) {
    out.write(kHemming, 4);
    if (!out) {
        throw std::runtime_error("write error");
    }

    uint32_t count = static_cast<uint32_t>(entries.size());
    Write(out, count);

    for (const auto& entry : entries) {
        if (entry.name.size() > 0xFFFF) {
            throw std::runtime_error("File name too long");
        }

        uint16_t nameLen = static_cast<uint16_t>(entry.name.size());
        Write(out, nameLen);
        if (nameLen > 0) {
            out.write(entry.name.data(), nameLen);
            if (!out) {
                throw std::runtime_error("write error");
            }
        }
        Write(out, entry.originalSize);
    }
}

void CopyBytes(std::istream& in, std::ostream& out, uint64_t bytesCount) {
    std::array<char, 8192> buffer{};

    while (bytesCount > 0) {
        std::streamsize chunk = static_cast<std::streamsize>(std::min<uint64_t>(buffer.size(), bytesCount));
        in.read(buffer.data(), chunk);
        if (in.gcount() != chunk) {
            throw std::runtime_error("Unexpected end of archive data");
        }

        out.write(buffer.data(), chunk);
        if (!out) {
            throw std::runtime_error("write error");
        }

        bytesCount -= static_cast<uint64_t>(chunk);
    }
}

void CopyStoredData(std::istream& in, std::ostream& out, const StoredEntry& entry) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(entry.dataOffset), std::ios::beg);
    if (!in) {
        throw std::runtime_error("Cannot seek archive data");
    }

    CopyBytes(in, out, EncodedSize(entry.originalSize));
}

void ReplaceArchiveFile(const std::string& tmpPath, const std::string& archivePath) {
    std::error_code ec;
    fs::rename(tmpPath, archivePath, ec);

    if (ec) {
        std::error_code removeError;
        fs::remove(archivePath, removeError);

        ec.clear();
        fs::rename(tmpPath, archivePath, ec);
    }

    if (ec) {
        throw std::runtime_error("Cannot replace archive");
    }
}

bool SamePathIfExists(const std::string& left, const std::string& right) {
    std::error_code ec;
    if (fs::exists(left, ec) && fs::exists(right, ec)) {
        return fs::equivalent(left, right, ec);
    }

    return fs::absolute(left).lexically_normal() == fs::absolute(right).lexically_normal();
}

} // namespace

Archive::Archive(std::string path) : path_(std::move(path)) {}

uint64_t Archive::GetFileSize(const std::string& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec) {
        throw std::runtime_error("Cannot get file size: " + path);
    }
    return static_cast<uint64_t>(sz);
}

void Archive::Create(const std::string files[], int fileCount) {
    if (fileCount <= 0) {
        throw std::runtime_error("No input files");
    }

    std::vector<FileEntry> entries;
    entries.reserve(fileCount);
    std::unordered_set<std::string> names;

    for (int i = 0; i < fileCount; ++i) {
        std::string name = ArchiveNameFromPath(files[i]);
        if (name.empty()) {
            throw std::runtime_error("Bad file name");
        }
        if (!names.insert(name).second) {
            throw std::runtime_error("Duplicate file name: " + name);
        }

        entries.push_back({name, GetFileSize(files[i])});
    }

    std::ofstream out(path_, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open archive");
    }

    WriteArchiveHeader(out, entries);

    for (int i = 0; i < fileCount; ++i) {
        std::ifstream in(files[i], std::ios::binary);
        if (!in) {
            throw std::runtime_error("Cannot open input: " + files[i]);
        }

        HammingEncodeStream(in, out, entries[i].originalSize);
    }
}

void Archive::List() {
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open archive");
    }

    std::vector<StoredEntry> entries = ReadArchiveIndex(in);

    for (const auto& entry : entries) {
        std::cout << entry.name << " " << entry.originalSize << " bytes" << '\n';
    }
}

void Archive::Extract(const std::string files[], int fileCount) {
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open archive");
    }

    std::vector<StoredEntry> entries = ReadArchiveIndex(in);

    std::unordered_set<std::string> requested;
    for (int i = 0; i < fileCount; ++i) {
        requested.insert(ArchiveNameFromPath(files[i]));
    }

    int extractedCount = 0;

    for (const auto& entry : entries) {
        if (!requested.empty() && requested.find(entry.name) == requested.end()) {
            continue;
        }

        in.clear();
        in.seekg(static_cast<std::streamoff>(entry.dataOffset), std::ios::beg);
        if (!in) {
            throw std::runtime_error("Cannot seek archive data");
        }

        std::ofstream out(entry.name, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open output: " + entry.name);
        }

        bool bad = HammingDecodeStream(in, out, entry.originalSize);
        if (bad) {
            std::cerr << "Warning: uncorrectable errors in " << entry.name << '\n';
        }

        ++extractedCount;
    }

    if (!requested.empty() && extractedCount == 0) {
        throw std::runtime_error("No requested files found in archive");
    }
}

void Archive::ExtractAll() {
    Extract(nullptr, 0);
}

void Archive::Append(const std::string files[], int fileCount) {
    if (fileCount <= 0) {
        throw std::runtime_error("No input files");
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open archive");
    }

    std::vector<StoredEntry> oldEntries = ReadArchiveIndex(in);

    std::vector<FileEntry> allEntries;
    allEntries.reserve(oldEntries.size() + fileCount);

    std::unordered_set<std::string> names;
    for (const auto& entry : oldEntries) {
        allEntries.push_back({entry.name, entry.originalSize});
        names.insert(entry.name);
    }

    std::vector<std::string> appendPaths;
    appendPaths.reserve(fileCount);

    for (int i = 0; i < fileCount; ++i) {
        std::string name = ArchiveNameFromPath(files[i]);
        if (name.empty()) {
            throw std::runtime_error("Bad file name");
        }
        if (!names.insert(name).second) {
            throw std::runtime_error("File already exists in archive: " + name);
        }

        allEntries.push_back({name, GetFileSize(files[i])});
        appendPaths.push_back(files[i]);
    }

    std::string tmpPath = path_ + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot create temporary archive");
    }

    WriteArchiveHeader(out, allEntries);

    for (const auto& entry : oldEntries) {
        CopyStoredData(in, out, entry);
    }

    for (std::size_t i = 0; i < appendPaths.size(); ++i) {
        std::ifstream fileIn(appendPaths[i], std::ios::binary);
        if (!fileIn) {
            throw std::runtime_error("Cannot open input: " + appendPaths[i]);
        }

        const FileEntry& entry = allEntries[oldEntries.size() + i];
        HammingEncodeStream(fileIn, out, entry.originalSize);
    }

    in.close();
    out.close();
    ReplaceArchiveFile(tmpPath, path_);
}

void Archive::Delete(const std::string files[], int fileCount) {
    if (fileCount <= 0) {
        throw std::runtime_error("No files for delete");
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open archive");
    }

    std::vector<StoredEntry> oldEntries = ReadArchiveIndex(in);

    std::unordered_set<std::string> toDelete;
    for (int i = 0; i < fileCount; ++i) {
        toDelete.insert(ArchiveNameFromPath(files[i]));
    }

    std::vector<StoredEntry> keptStoredEntries;
    std::vector<FileEntry> keptEntries;
    int deletedCount = 0;

    for (const auto& entry : oldEntries) {
        if (toDelete.find(entry.name) != toDelete.end()) {
            ++deletedCount;
            continue;
        }

        keptStoredEntries.push_back(entry);
        keptEntries.push_back({entry.name, entry.originalSize});
    }

    if (deletedCount == 0) {
        throw std::runtime_error("No such file in archive");
    }

    std::string tmpPath = path_ + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot create temporary archive");
    }

    WriteArchiveHeader(out, keptEntries);

    for (const auto& entry : keptStoredEntries) {
        CopyStoredData(in, out, entry);
    }

    in.close();
    out.close();
    ReplaceArchiveFile(tmpPath, path_);
}

void Archive::Concatenate(const std::string archives[], int archiveCount) {
    if (archiveCount < 2) {
        throw std::runtime_error("Need at least two archives");
    }

    for (int i = 0; i < archiveCount; ++i) {
        if (SamePathIfExists(archives[i], path_)) {
            throw std::runtime_error("Output archive must be different from input archives");
        }
    }

    std::vector<std::unique_ptr<std::ifstream>> streams;
    std::vector<SourceEntry> sourceEntries;
    std::vector<FileEntry> resultEntries;
    std::unordered_set<std::string> names;

    for (int i = 0; i < archiveCount; ++i) {
        auto in = std::make_unique<std::ifstream>(archives[i], std::ios::binary);
        if (!*in) {
            throw std::runtime_error("Cannot open archive: " + archives[i]);
        }

        std::size_t sourceIndex = streams.size();
        std::vector<StoredEntry> entries = ReadArchiveIndex(*in);

        for (const auto& entry : entries) {
            if (!names.insert(entry.name).second) {
                throw std::runtime_error("Duplicate file name in archives: " + entry.name);
            }

            sourceEntries.push_back({entry.name, entry.originalSize, entry.dataOffset, sourceIndex});
            resultEntries.push_back({entry.name, entry.originalSize});
        }

        streams.push_back(std::move(in));
    }

    std::ofstream out(path_, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot create output archive");
    }

    WriteArchiveHeader(out, resultEntries);

    for (const auto& entry : sourceEntries) {
        StoredEntry stored{entry.name, entry.originalSize, entry.dataOffset};
        CopyStoredData(*streams[entry.sourceArchive], out, stored);
    }
}
