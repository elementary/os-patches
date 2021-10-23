#!/bin/sh -e

echo "Importing $1/init-functions"
. $1/init-functions

log_warning_msg "Only a warning"
log_success_msg "This should succeed"
log_failure_msg "This fails miserably"

echo "OK!"

# Test pidofproc sanity checking.

echo "Testing pidofproc command line checks"

echo " Simple check, no options:"
pidofproc nonexistent
RETVAL=$?
if [ $RETVAL -ne 3 ]; then
    echo "Unexpected return value $RETVAL"
fi

echo " With -p option:"
pidofproc -p /var/run/nonexist.pid nonexistent
RETVAL=$?
if [ $RETVAL -ne 3 ]; then
    echo "Unexpected return value $RETVAL"
fi

echo " With -p option, but in wrong place:"
pidofproc nonexistent -p /var/run/nonexist.pid
RETVAL=$?
if [ $RETVAL -ne 4 ]; then
    echo "Unexpected return value $RETVAL"
fi

echo "OK!"

