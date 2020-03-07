#!/bin/bash

# Script to run cppcheck on project code base.
# Ignores ESP, etc. library files.

# Misra rules are run from checkproj-misra.sh

# Check the "main" folder's code.

cppcheck --template=gcc --cppcheck-build-dir=build --output-file=cppcheck.out main
