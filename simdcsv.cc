#include "simdcsv.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <iostream>
#include <string>

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

void simdcsv::parser::process_bitmask(uint32_t mask, size_t base_offset,
                                      std::vector<size_t> &positions) {
  while (mask) {
    int bit_pos = __builtin_ctz(mask);
    positions.push_back(base_offset + bit_pos);

    mask &= (mask - 1);
  }
}

void simdcsv::parser::parse() {
  const size_t CHUNK_SIZE = 32;

  for (size_t pos = 0; pos + CHUNK_SIZE <= buffer_size; pos += CHUNK_SIZE) {
    __m256i chunk =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buffer + pos));

    uint32_t comma_mask = find_commas(chunk);
    uint32_t newline_mask = find_newlines(chunk);
    uint32_t quote_mask = find_quotes(chunk);

    process_bitmask(comma_mask, pos, comma_pos);
    process_bitmask(newline_mask, pos, nl_pos);
  }

  size_t remaining_start = (buffer_size / CHUNK_SIZE) * CHUNK_SIZE;
  for (size_t pos = remaining_start; pos < buffer_size; ++pos) {
    char c = buffer[pos];

    if (c == ',') {
      comma_pos.push_back(pos);
    } else if (c == '\n') {
      nl_pos.push_back(pos);
    } else if (c == '"') {
      quote_pos.push_back(pos);
    }
  }
}

std::vector<std::vector<std::string>> simdcsv::parser::extract_fields() const {
  std::vector<std::vector<std::string>> result;

  if (nl_pos.empty()) {
    return result;
  }

  size_t row_start = 0;

  for (size_t nl_idx : nl_pos) {
    std::vector<std::string> row;
    size_t field_start = row_start;

    // Find commas in this row
    for (size_t comma_idx : comma_pos) {
      if (comma_idx > nl_idx)
        break;
      if (comma_idx >= row_start) {
        // Extract field
        size_t field_len = comma_idx - field_start;
        std::string field(buffer + field_start, field_len);
        row.push_back(field);
        field_start = comma_idx + 1;
      }
    }

    // Add the last field in the row
    if (field_start < nl_idx) {
      size_t field_len = nl_idx - field_start;
      std::string field(buffer + field_start, field_len);
      row.push_back(field);
    }

    result.push_back(row);
    row_start = nl_idx + 1;
  }

  return result;
}

void print_bitmask(uint32_t mask, const char *test_data) {
  printf("Test data: \"%.32s\"\n", test_data);
  printf("Bitmask:   0x%08X\n", mask);
  printf("Positions: ");

  for (int i = 0; i < 32; i++) {
    if (mask & (1U << i)) {
      printf("%d ", i);
    }
  }
  printf("\n");
}

int main() {
  // Test CSV data
  std::string csv_data = "name,age,city\n"
                         "Alice,25,New York\n"
                         "Bob,30,Los Angeles\n"
                         "Charlie,35,Chicago\n"
                         "Diana,28,Houston\n";

  simdcsv::parser parser(csv_data.c_str(), csv_data.size());

  parser.parse();

  auto rows = parser.extract_fields();

  std::cout << "Parsed CSV data:\n";
  std::cout << "Number of rows: " << rows.size() << "\n\n";

  for (size_t i = 0; i < rows.size(); ++i) {
    std::cout << "Row " << i << ": ";
    for (size_t j = 0; j < rows[i].size(); ++j) {
      std::cout << "\"" << rows[i][j] << "\"";
      if (j < rows[i].size() - 1) {
        std::cout << ", ";
      }
    }
    std::cout << "\n";
  }

  std::cout << "\n--- Testing edge cases ---\n";

  std::string empty_csv = "";
  simdcsv::parser empty_parser(empty_csv.c_str(), empty_csv.size());
  empty_parser.parse();
  auto empty_rows = empty_parser.extract_fields();
  std::cout << "Empty CSV rows: " << empty_rows.size() << "\n";

  std::string single_row = "field1,field2,field3\n";
  simdcsv::parser single_parser(single_row.c_str(), single_row.size());
  single_parser.parse();
  auto single_rows = single_parser.extract_fields();
  std::cout << "Single row CSV: " << single_rows.size() << " rows\n";
  if (!single_rows.empty()) {
    std::cout << "Fields: ";
    for (const auto &field : single_rows[0]) {
      std::cout << "\"" << field << "\" ";
    }
    std::cout << "\n";
  }

  std::string long_csv = "id,name,email,department,salary\n";
  for (int i = 1; i <= 20000; ++i) {
    long_csv += std::to_string(i) + ",Employee" + std::to_string(i) + ",emp" +
                std::to_string(i) + "@company.com,Engineering," +
                std::to_string(50000 + i * 1000) + "\n";
  }

  simdcsv::parser long_parser(long_csv.c_str(), long_csv.size());
  long_parser.parse();
  auto long_rows = long_parser.extract_fields();
  std::cout << "\nLong CSV test: " << long_rows.size() << " rows parsed\n";

  return 0;
}
