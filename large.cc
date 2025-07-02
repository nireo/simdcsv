#include "simdcsv.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

std::string read_file(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open file");
  }

  auto size = std::filesystem::file_size(path);
  std::string content(size, '\0');
  file.read(content.data(), size);

  return content;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "you need to provide a file path\n";
    return 1;
  }

  auto dump = argc == 3;
  try {
    auto content = read_file(argv[1]);
    auto start = std::chrono::high_resolution_clock::now();

    simdcsv::parser parser(content.c_str(), content.size());
    parser.parse();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Execution took: " << duration.count() << " ms" << std::endl;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
  }
}
