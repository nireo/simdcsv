#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace simdcsv {

// this just implements a very simple encoding for values such that the code is
// more memory efficient. meaning we only need a single vector to store all
// positions. this makes processing easier and the order is also easy to find
// since the commas are followed by newlines etc
struct position_entry {
  static constexpr uint64_t TYPE_MASK = 0x3ULL << 62;
  static constexpr uint64_t POS_MASK = ~TYPE_MASK;

  enum type : uint64_t {
    COMMA = 0ULL << 62,
    NEWLINE = 1ULL << 62,
    QUOTE = 2ULL << 62,
  };

  uint32_t data;

  position_entry(size_t pos, type t) : data(pos | static_cast<uint64_t>(t)) {}
  size_t position() const { return data & POS_MASK; }
  type get_type() const { return static_cast<type>(data & TYPE_MASK); }
};

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
