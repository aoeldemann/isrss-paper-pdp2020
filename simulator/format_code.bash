#!/bin/bash
find . -name "*.cc" -o -name "*.h" | xargs clang-format -i