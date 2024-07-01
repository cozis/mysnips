all:
	gcc log.c log_test.c      ../time/clock.c ../lockfree/spsc_queue.c ../thread/thread.c ../thread/sync.c -o log_test -Wall -Wextra -O2 -ggdb
	gcc log.c log_benchmark.c ../time/clock.c ../lockfree/spsc_queue.c ../thread/thread.c ../thread/sync.c ../time/profile.c -o log_benchmark -Wall -Wextra -O2 -DNDEBUG -ggdb #-DPROFILE
