# OpenGL Hash Functions
> Some hash function OpenCL kernels

This project contains a set of reference OpenCL implementations of common hash
functions and a simple application to drive them.

## Hash functions

| Function    | Implemented |
|-------------|-------------|
| MD5         | ✗           |
| SHA-1       | ✓           |
| SHA-224     | ✗           |
| SHA-256     | ✗           |
| SHA-384     | ✗           |
| SHA-512     | ✗           |
| SHA-512/224 | ✗           |
| SHA-512/256 | ✗           |
| SHA3-224    | ✗           |
| SHA3-256    | ✗           |
| SHA3-384    | ✗           |
| SHA3-512    | ✗           |

## Tester

This project provides a simple application to test the hash functions.

The application takes an input through stdin and prints the hash of each line.

### Pre-requisites

To build the tester, you will need `make`, a C compilers, and the OpenCL SDK.
To install the OpenCL SDK, check out
[this repo](https://github.com/KhronosGroup/OpenCL-Guide).

To run the tester, you will need to install the appropriate OpenCL runtime for
your target platform: Intel, AMD, or Nvidia.

### Building

1. Run `make`.

### Usage

```
Usage: ./hash [options] <hash_function>

Options:
  -h            Show this message
  -d <cpu/gpu>  Use CPU or GPU
  -p <n>        Use n-th platform
```

## Motivation

Recently, I've started delving into hash functions and finding them very
interesting.
I wanted to implement a few to better understand how they work and play around
with OpenCL.
Another goal of this project is to provide a reference OpenCL implementation
for these hash functions with a very permissive license.
My implementation is by no means optimal, but I hope it is at least legible.
