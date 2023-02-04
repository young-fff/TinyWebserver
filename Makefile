
CC=g++  
CXXFLAGS = -std=c++0x
CFLAGS=-I
webserver: ./main.o
##$(CC) -o ./main ./noactive/nonactive_conn.cpp ./http_conn/http_conn.cpp --std=c++11 -pthread 
##	$(CC) -o ./main ./main.cpp ./http_conn/http_conn.cpp --std=c++11 -pthread 	
	$(CC) -o main ./main.cpp ./http_conn/http_conn.cpp --std=c++11 -pthread 

	rm -f ./*.o

clean: 
	rm -f ./main
	rm -f ./*.o
