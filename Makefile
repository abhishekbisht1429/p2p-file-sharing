CPP = g++
CC = gcc

LIB = -pthread -ldl
tracker : tracker.cpp sqlite/sqlite3.o
	$(CPP) tracker.cpp sqlite/sqlite3.o -o tracker.o $(LIB)
	
sqlite/sqlite3.o : sqlite/sqlite3.c
	$(CC) -c sqlite/sqlite3.c -o sqlite/sqlite3.o

clean : 
	rm -f */*.o *.o
