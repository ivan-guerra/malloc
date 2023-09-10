# malloc

This project includes a header-only implementation of a barebones memory
allocator. The allocator uses [mmap][1] under the hood to acquire a pool of
memory from the OS. All user requests and bookkeeping data structures are
maintained within this mmaped memory region. When servicing allocation requests,
a first fit algorithm is used to find free blocks. Upon the release of memory
blocks, memory is automatically compacted.

### Building

To build the project locally, you will need the following libraries and tools
installed:

* CMake3.27+
* C++ compiler supporting C++20 features
* [Doxygen][2]

To build the project, change directory to the `scripts/` directory and run
`build.sh`.

### Running the Tests

`malloc` has been unit tested using the GoogleTest framework in conjunction with
`ctest`. To run the tests, build the project and change directory to
`malloc/build/`.  Run `ctest` to execute all unit tests.

### Doxygen Docs

The `malloc` API is documented using Doxygen. Doxygen docs are built
automatically by the build script. Docs can be found under
`malloc/docs/malloc/`.

[1]: https://linux.die.net/man/2/mmap
[2]: https://www.doxygen.nl/
