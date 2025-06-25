#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace simdcsv {
struct parser {
  std::vector<size_t> comma_pos;
  std::vector<size_t> nl_pos;
  std::vector<size_t> quote_pos;

  const char *buffer;
  size_t buffer_size;

  parser(const char *data, size_t len) : buffer(data), buffer_size(len) {}
  void process_bitmask(uint32_t mask, size_t base_offset,
                       std::vector<size_t> &positions);
  // this of course copies the values which is slow but this is just for testing
  std::vector<std::vector<std::string>> extract_fields() const;
  void parse();
};
} // namespace simdcsv
