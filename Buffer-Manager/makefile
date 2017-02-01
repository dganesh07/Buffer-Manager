CC = gcc
CFLAGS  = -g -Wall 


test1: test_assign2_1.o buffer_mgr.o  storage_mgr.o dberror.o
	$(CC) $(CFLAGS) -o test1 test_assign2_1.o buffer_mgr.o storage_mgr.o dberror.o

test_assign2_1.o: test_assign2_1.c dberror.h storage_mgr.h buffer_mgr.h test_helper.h
	$(CC) $(CFLAGS) -c test_assign2_1.c

dberror.o: dberror.c dberror.h 
	$(CC) $(CFLAGS) -c dberror.c

storage_mgr.o: storage_mgr.c storage_mgr.h 
	$(CC) $(CFLAGS) -c storage_mgr.c

buffer_mgr.o: buffer_mgr.c buffer_mgr.h
	$(CC) $(CFLAGS) -c buffer_mgr.c

clean: 
	$(RM) test1 *.o *~

run_test1:
	./test1