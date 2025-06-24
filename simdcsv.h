#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
namespace simdcsv {
struct parser {
  std::vector<size_t> comma_pos;
  std::vector<size_t> nl_pos;

  const char *buffer;
  size_t buffer_size;
  std::string_view get_field(size_t row, size_t col) const;
  void process_bitmask(uint32_t mask, size_t base_offset,
                       std::vector<size_t> &positions);
  void parse();
};
} // namespace simdcsv
