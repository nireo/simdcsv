#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
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

  uint64_t data;

  position_entry(size_t pos, type t) : data(pos | static_cast<uint64_t>(t)) {}
  size_t position() const { return data & POS_MASK; }
  type get_type() const { return static_cast<type>(data & TYPE_MASK); }
};

struct row;

struct parser {
  std::vector<position_entry> positions;
  const char *buffer;
  size_t buffer_size;

  parser(const char *data, size_t len) : buffer(data), buffer_size(len) {}

  static uint32_t find_commas(const __m256i chunk) {
    __m256i comma = _mm256_set1_epi8(',');
    __m256i cmp = _mm256_cmpeq_epi8(chunk, comma);
    return _mm256_movemask_epi8(cmp);
  }

  static uint32_t find_quotes(const __m256i chunk) {
    __m256i qt = _mm256_set1_epi8('"');
    __m256i cmp = _mm256_cmpeq_epi8(chunk, qt);
    return _mm256_movemask_epi8(cmp);
  }

  static uint32_t find_newlines(const __m256i chunk) {
    __m256i nl = _mm256_set1_epi8('\n');
    __m256i cmp = _mm256_cmpeq_epi8(chunk, nl);
    return _mm256_movemask_epi8(cmp);
  }

  void process_bitmask(uint32_t mask, size_t base_offset,
                       position_entry::type entry_type) {
    while (mask) {
      int bit_pos = __builtin_ctz(mask);
      positions.emplace_back(base_offset + bit_pos, entry_type);
      mask &= (mask - 1);
    }
  }

  // for each chunk, we execute 10 SIMD instructions (1 load + 3×3 for delimiter
  // detection) to find all commas, newlines, and quotes simultaneously, rather
  // than checking each byte individually which would require more comparisons.
  void parse() {
    const size_t CHUNK_SIZE = 32;
    positions.reserve(buffer_size / 10);

    for (size_t pos = 0; pos + CHUNK_SIZE <= buffer_size; pos += CHUNK_SIZE) {
      __m256i chunk =
          _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + pos));

      uint32_t comma_mask = find_commas(chunk);
      uint32_t newline_mask = find_newlines(chunk);
      uint32_t quote_mask = find_quotes(chunk);

      process_bitmask(comma_mask, pos, position_entry::COMMA);
      process_bitmask(newline_mask, pos, position_entry::NEWLINE);
      process_bitmask(quote_mask, pos, position_entry::QUOTE);
    }

    size_t remaining_start = (buffer_size / CHUNK_SIZE) * CHUNK_SIZE;
    for (size_t pos = remaining_start; pos < buffer_size; ++pos) {
      char c = buffer[pos];
      if (c == ',') {
        positions.emplace_back(pos, position_entry::COMMA);
      } else if (c == '\n') {
        positions.emplace_back(pos, position_entry::NEWLINE);
      } else if (c == '"') {
        positions.emplace_back(pos, position_entry::QUOTE);
      }
    }

    std::sort(positions.begin(), positions.end(),
              [](const position_entry &a, const position_entry &b) {
                return a.position() < b.position();
              });
  }

  std::vector<size_t> get_row_boundaries() const {
    std::vector<size_t> boundaries;
    boundaries.push_back(0);

    for (const auto &pos : positions) {
      if (pos.get_type() == position_entry::NEWLINE) {
        boundaries.push_back(pos.position() + 1);
      }
    }

    if (boundaries.empty() || boundaries.back() < buffer_size) {
      boundaries.push_back(buffer_size);
    }

    return boundaries;
  }

  struct row_iterator {
    const parser *parser_;
    std::vector<size_t> row_boundaries_;
    size_t current_row_index_;

    row_iterator(const parser *p, size_t row_idx)
        : parser_(p), row_boundaries_(p->get_row_boundaries()),
          current_row_index_(row_idx) {}

    row operator*() const;

    row_iterator &operator++() {
      current_row_index_++;
      return *this;
    }

    bool operator!=(const row_iterator &other) const {
      return current_row_index_ != other.current_row_index_;
    }

    bool operator==(const row_iterator &other) const {
      return current_row_index_ == other.current_row_index_;
    }
  };

  row_iterator begin() const { return row_iterator(this, 0); }
  row_iterator end() const {
    auto boundaries = get_row_boundaries();
    return row_iterator(this, boundaries.size() - 1);
  }
};

struct row {
  const parser *parser_;
  size_t row_start_pos_;
  size_t row_end_pos_;

  row(const parser *p, size_t start_pos, size_t end_pos)
      : parser_(p), row_start_pos_(start_pos), row_end_pos_(end_pos) {}

  std::string_view operator[](size_t field_index) const {
    std::vector<size_t> comma_positions;

    for (const auto &pos : parser_->positions) {
      if (pos.position() >= row_start_pos_ && pos.position() < row_end_pos_) {
        if (pos.get_type() == position_entry::COMMA) {
          comma_positions.push_back(pos.position());
        }
      }
    }

    size_t field_start, field_end;

    if (field_index == 0) {
      field_start = row_start_pos_;
    } else if (field_index - 1 < comma_positions.size()) {
      field_start = comma_positions[field_index - 1] + 1;
    } else {
      return {};
    }

    if (field_index < comma_positions.size()) {
      field_end = comma_positions[field_index];
    } else {
      field_end = row_end_pos_;
      if (field_end > 0 && parser_->buffer[field_end - 1] == '\n') {
        field_end--;
      }
    }

    if (field_start >= field_end)
      return {};

    return std::string_view(parser_->buffer + field_start,
                            field_end - field_start);
  }

  size_t size() const {
    size_t comma_count = 0;
    for (const auto &pos : parser_->positions) {
      if (pos.position() >= row_start_pos_ && pos.position() < row_end_pos_) {
        if (pos.get_type() == position_entry::COMMA) {
          comma_count++;
        }
      }
    }
    return comma_count + 1;
  }

  struct field_iterator {
    const row *row_;
    size_t field_index_;

    field_iterator(const row *row, size_t field_idx)
        : row_(row), field_index_(field_idx) {}

    std::string_view operator*() const { return (*row_)[field_index_]; }

    field_iterator &operator++() {
      field_index_++;
      return *this;
    }

    bool operator!=(const field_iterator &other) const {
      return field_index_ != other.field_index_;
    }

    bool operator==(const field_iterator &other) const {
      return field_index_ == other.field_index_;
    }
  };

  field_iterator begin() const { return field_iterator(this, 0); }
  field_iterator end() const { return field_iterator(this, size()); }
};

} // namespace simdcsv
