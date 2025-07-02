#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace simdcsv {

struct parser {
  std::vector<uint64_t> comma_pos;
  std::vector<uint64_t> nl_pos;
  std::vector<uint64_t> quote_pos;

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
                       std::vector<uint64_t> &pos_vector) {
    while (mask) {
      int bit_pos = __builtin_ctz(mask);
      pos_vector.emplace_back(base_offset + bit_pos);
      mask &= (mask - 1);
    }
  }

  void parse() {
    const size_t CHUNK_SIZE = 32;
    comma_pos.reserve(buffer_size / 8);
    nl_pos.reserve(buffer_size / 128);

    for (size_t pos = 0; pos + CHUNK_SIZE <= buffer_size; pos += CHUNK_SIZE) {
      __m256i chunk =
          _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + pos));

      uint32_t comma_mask = find_commas(chunk);
      uint32_t newline_mask = find_newlines(chunk);
      uint32_t quote_mask = find_quotes(chunk);

      process_bitmask(comma_mask, pos, comma_pos);
      process_bitmask(newline_mask, pos, nl_pos);
      process_bitmask(quote_mask, pos, quote_pos);
    }

    size_t remaining_start = (buffer_size / CHUNK_SIZE) * CHUNK_SIZE;
    for (size_t pos = remaining_start; pos < buffer_size; ++pos) {
      char c = buffer[pos];
      if (c == ',') {
        comma_pos.emplace_back(pos);
      } else if (c == '\n') {
        nl_pos.emplace_back(pos);
      } else if (c == '"') {
        quote_pos.emplace_back(pos);
      }
    }
  }

  struct row {
    const parser *parser_;
    size_t row_start_;
    size_t row_end_;
    mutable std::vector<size_t> field_boundaries_;
    mutable bool boundaries_computed_ = false;

    row(const parser *p, size_t start, size_t end)
        : parser_(p), row_start_(start), row_end_(end) {}

    void compute_boundaries() const {
      if (boundaries_computed_)
        return;

      field_boundaries_.clear();
      field_boundaries_.push_back(row_start_);

      auto comma_begin = std::lower_bound(parser_->comma_pos.begin(),
                                          parser_->comma_pos.end(), row_start_);
      auto comma_end =
          std::lower_bound(comma_begin, parser_->comma_pos.end(), row_end_);

      for (auto it = comma_begin; it != comma_end; ++it) {
        field_boundaries_.push_back(*it + 1);
      }

      size_t actual_end = row_end_;
      if (actual_end > row_start_ && parser_->buffer[actual_end - 1] == '\n') {
        actual_end--;
      }
      field_boundaries_.push_back(actual_end);
      boundaries_computed_ = true;
    }

    std::string_view operator[](size_t field_index) const {
      compute_boundaries();

      if (field_index + 1 >= field_boundaries_.size()) {
        return {};
      }

      size_t start = field_boundaries_[field_index];
      size_t end = field_boundaries_[field_index + 1];

      if (field_index + 1 < field_boundaries_.size() - 1) {
        end = field_boundaries_[field_index + 1] - 1;
      }

      if (start >= end)
        return {};

      return std::string_view(parser_->buffer + start, end - start);
    }

    size_t size() const {
      compute_boundaries();
      return field_boundaries_.empty() ? 0 : field_boundaries_.size() - 1;
    }

    struct field_iterator {
      const row *row_;
      size_t field_index_;

      field_iterator(const row *r, size_t idx) : row_(r), field_index_(idx) {}

      std::string_view operator*() const { return (*row_)[field_index_]; }

      field_iterator &operator++() {
        ++field_index_;
        return *this;
      }

      field_iterator operator++(int) {
        field_iterator tmp = *this;
        ++field_index_;
        return tmp;
      }

      bool operator==(const field_iterator &other) const {
        return field_index_ == other.field_index_;
      }

      bool operator!=(const field_iterator &other) const {
        return !(*this == other);
      }
    };

    field_iterator begin() const { return field_iterator(this, 0); }
    field_iterator end() const { return field_iterator(this, size()); }
  };

  struct row_iterator {
    const parser *parser_;
    size_t current_nl_index_;

    row_iterator(const parser *p, size_t nl_idx)
        : parser_(p), current_nl_index_(nl_idx) {}
    row operator*() const {
      size_t row_start = (current_nl_index_ == 0)
                             ? 0
                             : parser_->nl_pos[current_nl_index_ - 1] + 1;
      size_t row_end = (current_nl_index_ < parser_->nl_pos.size())
                           ? parser_->nl_pos[current_nl_index_] + 1
                           : parser_->buffer_size;
      return row(parser_, row_start, row_end);
    }

    row_iterator &operator++() {
      ++current_nl_index_;
      return *this;
    }

    row_iterator operator++(int) {
      row_iterator tmp = *this;
      ++current_nl_index_;
      return tmp;
    }

    bool operator==(const row_iterator &other) const {
      return current_nl_index_ == other.current_nl_index_;
    }

    bool operator!=(const row_iterator &other) const {
      return !(*this == other);
    }
  };

  row_iterator begin() const { return row_iterator(this, 0); }

  row_iterator end() const {
    size_t num_rows = nl_pos.empty() ? 1 : nl_pos.size() + 1;
    return row_iterator(this, num_rows);
  }

  template <typename Callback> void for_each_row(Callback &&callback) const {
    size_t row_start = 0;

    for (size_t nl_idx = 0; nl_idx < nl_pos.size(); ++nl_idx) {
      size_t row_end = nl_pos[nl_idx] + 1;
      row current_row(this, row_start, row_end);

      if (!callback(current_row)) {
        break;
      }

      row_start = row_end;
    }

    if (row_start < buffer_size) {
      row current_row(this, row_start, buffer_size);
      callback(current_row);
    }
  }
};
} // namespace simdcsv
