# Project Helesta

[![Build](https://github.com/helesta-compiler/helesta/actions/workflows/check.yml/badge.svg)](https://github.com/helesta-compiler/helesta/actions/workflows/check.yml)

## Quick Start

```sh
git clone git@github.com:helesta-compiler/helesta.git
cd helesta && mkdir build && cd build
# Configure
cmake ..
# Build
make -j6
# Format (Optional)
make format
# Format Check (Optional)
make format_check
```

Every push to this repo triggers GitHub workflow to verity the sanity of the code. You can add `$bench` to commit message to generate benchmark summary in the action page. One integral benchmark costs around 1 hour. Use `$bench` with `$fast` in the commit message to reduce the running time to 20 minutes. Fast benchmark simply ignores the testcases for correctness and skip benchmark for other reference compilers.

## Setup

### Parser Generator

Due to the limitation of the contest, this project uses ANTLR4 with version 4.8. Please follow the instructions on the [ANTLR](https://www.antlr.org) website to download and install the antlr toolchain and download the cpp-runtime.

## Submit

This contest uses a weird way to compile codes. Please use `flat_includes.py` to process the source code to submit.
