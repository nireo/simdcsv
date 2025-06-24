#include <string_view>
#include <vector>
namespace simdcsv {
struct parser {
  std::vector<size_t> field_starts;
  std::vector<size_t> field_ends;
  std::vector<size_t> row_starts;

  const char *buffer;
  size_t buffer_size;
  std::string_view get_field(size_t row, size_t col) const;
};
} // namespace simdcsv
