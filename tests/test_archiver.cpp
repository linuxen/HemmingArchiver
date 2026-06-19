#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <chrono>


namespace fs = std::filesystem;

static std::string QuotePath(const fs::path& p) {
	fs::path native_path = p;
	native_path.make_preferred();
	std::string s = native_path.string();
#ifdef _WIN32
	if (s.find(' ') != std::string::npos) {
		return "\"" + s + "\"";
	}
	return s;
#else
	return "\"" + s + "\"";
#endif
}

static bool FilesEqual(const fs::path& a, const fs::path& b) {
	if (!fs::exists(a) || !fs::exists(b)) return false;
	if (fs::file_size(a) != fs::file_size(b)) return false;

	std::ifstream fa(a, std::ios::binary);
	std::ifstream fb(b, std::ios::binary);
	if (!fa || !fb) return false;

	const std::size_t kBufferSize = 1 << 20;
	std::vector<char> ba(kBufferSize), bb(kBufferSize);

	while (fa && fb) {
		fa.read(ba.data(), static_cast<std::streamsize>(ba.size()));
		fb.read(bb.data(), static_cast<std::streamsize>(bb.size()));
		const auto ca = fa.gcount();
		const auto cb = fb.gcount();
		if (ca != cb) return false;
		if (std::memcmp(ba.data(), bb.data(), static_cast<std::size_t>(ca)) != 0) return false;
	}
	return fa.eof() && fb.eof();
}

TEST(HamArcCLI, CreateAndExtractAndCompare) {
	const fs::path resourcesdir = fs::path(RESOURCES_DIR);
	const fs::path file1 = resourcesdir / "BjarneStroustrup.jpg";
	const fs::path file2 = resourcesdir / "Book.pdf";

	ASSERT_TRUE(fs::exists(file1));
	ASSERT_TRUE(fs::exists(file2));

	const auto temproot = fs::temp_directory_path();
	const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path work = temproot / ("hamarc_test_" + std::to_string(now));
	const fs::path outdir = work / "out";
	ASSERT_TRUE(fs::create_directories(outdir));

	const fs::path archive = work / "archive.haf";
	const std::string hamarc = HAMARC_EXE_PATH;

	{
		std::ostringstream cmd;
		cmd << QuotePath(hamarc)
		    << " --create"
		    << " --file=" << QuotePath(archive)
		    << " " << QuotePath(file1)
		    << " " << QuotePath(file2);
		std::cout << "Create command: " << cmd.str() << std::endl;
		const int rc = std::system(cmd.str().c_str());
		std::cout << "Return code: " << rc << std::endl;
		ASSERT_EQ(rc, 0);
		ASSERT_TRUE(fs::exists(archive));
	}

	{
		const std::uintmax_t archive_size = fs::file_size(archive);
		std::fstream archive_file(archive, std::ios::in | std::ios::out | std::ios::binary);
		ASSERT_TRUE(archive_file.is_open());

		std::vector<std::pair<std::uintmax_t, int>> flipped_bits = {
			{100, 0},
			{archive_size / 2, 0},
			{archive_size - 1, 0}
		};

		for (const auto& [byte_pos, bit_pos] : flipped_bits) {
			archive_file.seekg(static_cast<std::streamoff>(byte_pos));
			char byte = 0;
			archive_file.read(&byte, 1);
			if (archive_file.gcount() != 1) continue;

			byte ^= (1 << bit_pos);

			archive_file.seekp(static_cast<std::streamoff>(byte_pos));
			archive_file.write(&byte, 1);
			archive_file.flush();
		}

		archive_file.close();
	}

	{
		const fs::path original_dir = fs::current_path();
		fs::current_path(outdir);
		
		std::ostringstream cmd;
		cmd << QuotePath(hamarc)
		    << " --extract"
		    << " --file=" << QuotePath(fs::path("..") / archive.filename());
		const int rc = std::system(cmd.str().c_str());
		
		fs::current_path(original_dir);
		ASSERT_EQ(rc, 0);
	}

	const fs::path extr1 = outdir / file1.filename();
	const fs::path extr2 = outdir / file2.filename();
	ASSERT_TRUE(fs::exists(extr1));
	ASSERT_TRUE(fs::exists(extr2));

	EXPECT_TRUE(FilesEqual(file1, extr1));
	EXPECT_TRUE(FilesEqual(file2, extr2));
}
