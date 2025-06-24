
#include "simdcsv.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>

static uint32_t find_commas(const __m256i chunk) {
  __m256i comma = _mm256_set1_epi8(',');
  __m256i cmp = _mm256_cmpeq_epi8(chunk, comma);

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
    int bit_pos = __builtin_ctz(mask); // Count trailing zeros
    positions.push_back(base_offset + bit_pos);

    mask &= (mask - 1);
  }
}

void simdcsv::parser::parse() {
  const size_t CHUNK_SIZE = 32;

  for (size_t pos = 0; pos + CHUNK_SIZE <= buffer_size; pos += CHUNK_SIZE) {
    __m256i chunk = _mm256_loadu_si256((__m256i *)(buffer + pos));

    uint32_t comma_mask = find_commas(chunk);
    uint32_t newline_mask = find_newlines(chunk);

    process_bitmask(comma_mask, pos, comma_pos);
    process_bitmask(newline_mask, pos, nl_pos);
  }
  // TODO: handle the rest of the bytes that are not aligned to 32
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
  char test_data[32];

  memset(test_data, 0, 32);
  strcpy(test_data, "hello,world,test,data");
  __m256i chunk1 = _mm256_loadu_si256((__m256i *)test_data);
  uint32_t result1 = find_commas(chunk1);
  printf("Test 1 - CSV data:\n");
  print_bitmask(result1, test_data);

  memset(test_data, 0, 32);
  strcpy(test_data, "no commas in this string");
  __m256i chunk2 = _mm256_loadu_si256((__m256i *)test_data);
  uint32_t result2 = find_commas(chunk2);
  printf("Test 2 - No commas:\n");
  print_bitmask(result2, test_data);

  memset(test_data, 0, 32);
  strcpy(test_data, "a,,b,,,c,,,,d");
  __m256i chunk3 = _mm256_loadu_si256((__m256i *)test_data);
  uint32_t result3 = find_commas(chunk3);
  printf("Test 3 - Multiple commas:\n");
  print_bitmask(result3, test_data);

  memset(test_data, 0, 32);
  strcpy(test_data, ",start,middle,end,");
  __m256i chunk4 = _mm256_loadu_si256((__m256i *)test_data);
  uint32_t result4 = find_commas(chunk4);
  printf("Test 4 - Edge commas:\n");
  print_bitmask(result4, test_data);

  memset(test_data, 'x', 32);
  test_data[5] = ',';
  test_data[10] = ',';
  test_data[15] = ',';
  test_data[20] = ',';
  test_data[25] = ',';
  test_data[30] = ',';
  test_data[31] = '\0';

  __m256i chunk5 = _mm256_loadu_si256((__m256i *)test_data);
  uint32_t result5 = find_commas(chunk5);
  printf("Test 5 - Pattern test:\n");
  print_bitmask(result5, test_data);

  return 0;
}
