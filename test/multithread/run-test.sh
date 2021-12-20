cd $B/..
timeout 6s ./main | awk -f ./test/multithread/1.fn