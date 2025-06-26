#include "simdcsv.h"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <iostream>
#include <string>

simdcsv::row simdcsv::parser::row_iterator::operator*() const {
  if (current_row_index_ >= row_boundaries_.size() - 1) {
    return row(parser_, parser_->buffer_size, parser_->buffer_size);
  }

  size_t start_pos = row_boundaries_[current_row_index_];
  size_t end_pos = row_boundaries_[current_row_index_ + 1];

  return row(parser_, start_pos, end_pos);
}

bool test_basic_csv() {
  std::cout << "Testing basic CSV..." << std::endl;

  std::string csv_data = "name,age,city\nJohn,25,NYC\nJane,30,LA\n";
  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  std::vector<std::vector<std::string>> expected = {
      {"name", "age", "city"}, {"John", "25", "NYC"}, {"Jane", "30", "LA"}};

  size_t row_idx = 0;
  for (const auto &row : p) {
    if (expected.size() == row_idx) {
      break;
    }

    assert(row.size() == expected[row_idx].size());
    size_t field_idx = 0;
    for (const auto &field : row) {
      assert(field == expected[row_idx][field_idx]);
      field_idx++;
    }
    row_idx++;
  }

  std::cout << "Basic CSV test passed" << std::endl;
  return true;
}

bool test_single_row() {
  std::cout << "Testing single row..." << std::endl;

  std::string csv_data = "hello,world,test\n";
  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  size_t row_count = 0;
  for (const auto &row : p) {
    assert(row[0] == "hello");
    assert(row[1] == "world");
    assert(row[2] == "test");
    break;
  }

  std::cout << "Single row test passed" << std::endl;
  return true;
}

bool test_no_trailing_newline() {
  std::cout << "Testing no trailing newline..." << std::endl;

  std::string csv_data = "a,b,c\n1,2,3"; // No newline at end
  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  std::vector<std::vector<std::string>> expected = {{"a", "b", "c"},
                                                    {"1", "2", "3"}};

  size_t row_idx = 0;
  for (const auto &row : p) {
    assert(row_idx < expected.size());
    assert(row.size() == expected[row_idx].size());

    for (size_t i = 0; i < row.size(); ++i) {
      assert(row[i] == expected[row_idx][i]);
    }
    row_idx++;
  }

  std::cout << "No trailing newline test passed" << std::endl;
  return true;
}

bool test_large_csv() {
  std::cout << "Testing large CSV (>32 bytes for SIMD)..." << std::endl;

  std::string csv_data = "col1,col2,col3,col4,col5,col6,col7,col8,col9,col10\n";
  csv_data += "val1,val2,val3,val4,val5,val6,val7,val8,val9,val10\n";
  csv_data += "data1,data2,data3,data4,data5,data6,data7,data8,data9,data10\n";

  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  size_t row_count = 0;
  for (const auto &row : p) {
    assert(row.size() == 10);

    if (row_count == 0) {
      assert(row[0] == "col1");
      assert(row[9] == "col10");
    } else if (row_count == 1) {
      assert(row[0] == "val1");
      assert(row[9] == "val10");
    } else if (row_count == 2) {
      assert(row[0] == "data1");
      assert(row[9] == "data10");
    }
    row_count++;
  }
  assert(row_count == 3);

  std::cout << "Large CSV test passed" << std::endl;
  return true;
}

bool test_special_characters() {
  std::cout << "Testing special characters..." << std::endl;

  std::string csv_data = "hello world,123!@#,test$%^&*()\nfoo bar,456,baz\n";
  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  std::vector<std::vector<std::string>> expected = {
      {"hello world", "123!@#", "test$%^&*()"}, {"foo bar", "456", "baz"}};

  size_t row_idx = 0;
  for (const auto &row : p) {
    assert(row_idx < expected.size());
    assert(row.size() == expected[row_idx].size());

    for (size_t i = 0; i < row.size(); ++i) {
      assert(row[i] == expected[row_idx][i]);
    }
    row_idx++;
  }

  std::cout << "Special characters test passed" << std::endl;
  return true;
}

bool test_iterator_functionality() {
  std::cout << "Testing iterator functionality..." << std::endl;

  std::string csv_data = "a,b\nc,d\ne,f\n";
  simdcsv::parser p(csv_data.c_str(), csv_data.size());
  p.parse();

  auto it = p.begin();
  auto end = p.end();

  assert(it != end);
  auto row1 = *it;
  assert(row1.size() == 2);
  assert(row1[0] == "a");
  assert(row1[1] == "b");

  ++it;
  assert(it != end);
  auto row2 = *it;
  assert(row2.size() == 2);
  assert(row2[0] == "c");
  assert(row2[1] == "d");

  ++it;
  assert(it != end);
  auto row3 = *it;
  assert(row3.size() == 2);
  assert(row3[0] == "e");
  assert(row3[1] == "f");

  ++it;
  assert(it == end);

  std::cout << "Iterator functionality test passed" << std::endl;
  return true;
}

int main() {
  std::cout << "Running CSV Parser Tests..." << std::endl;
  std::cout << "=============================" << std::endl;

  try {
    test_basic_csv();
    test_single_row();
    test_no_trailing_newline();
    test_large_csv();
    test_special_characters();
    test_iterator_functionality();

    std::cout << std::endl;
    std::cout << "ALL TESTS PASSED!" << std::endl;
    std::cout << "Your CSV parser is working correctly!" << std::endl;

  } catch (const std::exception &e) {
    std::cout << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cout << "Test failed with unknown exception" << std::endl;
    return 1;
  }

  return 0;
}
