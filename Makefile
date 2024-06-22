
all:
	gcc thread/test_thread.c thread/thread.c time/clock.c -o test_thread -Wall -Wextra -ggdb
	gcc thread/test_mutex.c thread/thread.c thread/sync.c time/clock.c -o test_mutex -Wall -Wextra -ggdb

clean:
	rm test_mutex test_mutex.exe \
	   test_thread test_thread.exe