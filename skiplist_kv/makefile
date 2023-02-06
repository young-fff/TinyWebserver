## Version1
#	CC=g++  
#	CXXFLAGS = -std=c++0x
#	CFLAGS=-I
#	skiplist: main.o 
#		$(CC) -o ./bin/main main.o --std=c++11 -pthread 
#		rm -f ./*.o

#	clean: 
#		rm -f ./*.o

## Version2
CC=g++  
CXXFLAGS = -std=c++0x
CFLAGS=-I
skiplist: ./src/main.o
	$(CC) -o ./bin/main ./src/main.cpp --std=c++11 -pthread 
	rm -f ./src/*.o

clean: 
	rm -f ./bin/main
	rm -f ./main
	rm -f ./*.o
