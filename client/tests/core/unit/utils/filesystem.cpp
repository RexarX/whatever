#include <doctest/doctest.h>

#include <client/core/utils/filesystem.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>

TEST_SUITE("client::utils::Filesystem") {
  TEST_CASE("ReadFileToString: string_view overload") {
    constexpr std::string_view file_name = "client_test_file.txt";
    constexpr std::string_view file_content = "Hello, Client!";
    {
      std::ofstream out(file_name.data(), std::ofstream::trunc);
      out << file_content;
    }

    SUBCASE("File exists") {
      const auto result = client::utils::ReadFileToString(file_name);
      CHECK(result.has_value());
      CHECK_EQ(result.value(), file_content);
    }

    SUBCASE("File does not exist") {
      std::filesystem::remove(file_name);
      const auto result = client::utils::ReadFileToString(file_name);
      CHECK_FALSE(result.has_value());
      CHECK_EQ(result.error(), client::utils::FileError::kCouldNotOpen);
    }

    // Clean up if not already removed
    std::filesystem::remove(file_name);
  }

  TEST_CASE("ReadFileToString: filesystem::path overload") {
    constexpr std::string_view file_name = "client_test_file2.txt";
    constexpr std::string_view file_content = "Another test!";
    {
      std::ofstream out(file_name.data(), std::ofstream::trunc);
      out << file_content;
    }
    std::filesystem::path file_path(file_name);

    SUBCASE("File exists") {
      const auto result = client::utils::ReadFileToString(file_path);
      CHECK(result.has_value());
      CHECK_EQ(result.value(), file_content);
    }

    SUBCASE("File does not exist") {
      std::filesystem::remove(file_path);
      const auto result = client::utils::ReadFileToString(file_path);
      CHECK_FALSE(result.has_value());
      CHECK_EQ(result.error(), client::utils::FileError::kCouldNotOpen);
    }

    // Clean up if not already removed
    std::filesystem::remove(file_path);
  }

  TEST_CASE("GetFileName") {
    SUBCASE("Path with directories") {
      constexpr std::string_view path = "foo/bar/baz.txt";
      CHECK_EQ(client::utils::GetFileName(path), "baz.txt");
    }

    SUBCASE("Path without directories") {
      constexpr std::string_view path = "baz.txt";
      CHECK_EQ(client::utils::GetFileName(path), "baz.txt");
    }

    SUBCASE("Directory path ending with slash") {
      constexpr std::string_view path = "/tmp/dir/";
      CHECK_EQ(client::utils::GetFileName(path), "");
    }
  }

  TEST_CASE("GetFileExtension") {
    SUBCASE("Simple extension") {
      constexpr std::string_view path = "foo.txt";
      CHECK_EQ(client::utils::GetFileExtension(path), ".txt");
    }

    SUBCASE("Multiple dots") {
      constexpr std::string_view path = "bar.tar.gz";
      CHECK_EQ(client::utils::GetFileExtension(path), ".gz");
    }

    SUBCASE("No extension") {
      constexpr std::string_view path = "noext";
      CHECK_EQ(client::utils::GetFileExtension(path), "");
    }
  }

}  // TEST_SUITE
