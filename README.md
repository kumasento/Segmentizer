# Segmentizer

A useless (?) clang plugin that separates input C++ source into multiple segments and compiles them differently.

## Objective

```c++
int main() {
  Tensor W1('weights1.bin');
  Tensor W2('weights2.bin');
  Tensor B('bias.bin');
  Tensor X('input.bin');

#pragma start_dsl 
  Tensor Y = Softmax(Matmul(W2, Add(Matmul(W1, X), B)));

  /* MLIR generated from the frontend
  func @f(%arg0: tensor<?xf32>, ...) {
    %0 = rb.matmul(%arg0, %arg1)
    %1 = rb.add(...)
    %2 = rb.matmul(...)
    %3 = rb.softmax(...)
    return %3
  }
  */

  /* After rewrite 
  extern f(...);
  Tensor Y = f(X, W1, W2, B);
  */

#pragma end_dsl

  Y.print();
}
```

## Setup

The following instructions are only tested on Ubuntu 20.04.

Install the latest LLVM [[source](https://apt.llvm.org/)] -

```
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
```

Use CMake (at least 3.10) to build:

```
mkdir build
cd build
cmake .. -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12 -DCMAKE_BUILD_TYPE=DEBUG
```

## Run

```
cd build
make && clang++-12 -cc1 -load ./lib/ExtractCodeSegment.so -plugin extract-code-segment  ../test/ExtractCodeSegment/add.cc
```

Outputs:

```
Handling start_segment ...
Handling end_segment ...
Handling start_segment ...
Handling end_segment ...
../test/ExtractCodeSegment/add.cc
#pragma start_segment
  for (int i = 0; i < 10; i++) {
  }
#pragma end_segment
../test/ExtractCodeSegment/add.cc
#pragma start_segment
  while (true) {
  }
#pragma end_segment
```
