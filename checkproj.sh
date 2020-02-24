# Script to run cppcheck on project code base.
# Ignores ESP, etc. library files.

# Check the "main" folder's code.

#cppcheck --template=gcc --cppcheck-build-dir=cppcheck --output-file=cppcheck.out main

#cppcheck --template=gcc --addon=misra.py main

cppcheck --dump main
python /snap/cppcheck/current/config/addons/misra.py --rule-texts=~/Documents/Development/StaticAnalysis/misra2012-Rules.txt main/*.dump
