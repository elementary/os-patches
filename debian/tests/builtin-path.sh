#!/bin/bash
set -e
expected_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
actual_path=$(env -u PATH /bin/bash -c 'echo $PATH')
if [ "$expected_path" == "$actual_path" ]
then
    echo OK
else
    echo FAIL, expected
    echo $expected_path
    echo but got:
    echo $actual_path
    exit 1
fi
