# simdcsv

A CSV parser for C++ 17 which utilizes simd to improve performance.

How it works:
* Parse structures by storing the positions and offsets
* Data can be accessed on demand via views to reduce allocations
* Types are later converted lazily when needed 

This process minimizes memory allocations and makes the parser be able to parse gigabytes of CSV data very fast.

