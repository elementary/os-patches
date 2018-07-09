#!/bin/sh

set -e

for p in test_*.py; do
    echo "Running: $p"
    PYTHONPATH=.. ${PYTHON:-python} $p
done

# cleanup 
find ./test-data/var/lib/apt/ -type f | xargs rm -f
