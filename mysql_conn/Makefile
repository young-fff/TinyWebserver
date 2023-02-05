CC=g++  
CXXFLAGS = -std=c++0x
CFLAGS=-I
conn: ./main.o
	$(CC) -o main *.cpp --std=c++11 -pthread -ljsoncpp -lmysqlclient
	rm -f ./*.o

clean: 
	rm -f ./main
	rm -f ./*.o