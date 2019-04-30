#
# Verify that the fifos are empty and exit with a zero status if no errors else an error count that
# doesn't overflow what can safely be represented in the exit status.
#
empty_fifos
if (( $error_count == 0 ))
then
    cd /tmp
    rm -rf $TEST_DIR
    exit 0
else
    'log_warning' $((start_of_test_lineno - 1)) "error_count = $error_count"
    exit $(( error_count < 125 ? error_count : 125 ))
fi
