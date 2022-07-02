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

## Setup

### Parser Generator

Due to the limitation of the contest, this project uses ANTLR4 with version 4.8. Please follow the instructions on the [ANTLR](https://www.antlr.org) website to download and install the antlr toolchain and download the cpp-runtime.
