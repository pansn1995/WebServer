#	g++ paramenter
CC=g++
CCFLAGS=-std=c++11 -g -O3 -Wall -D_PTHREADS -finput-charset=UTF-8 -fexec-charset=UTF-8 -static
CCLINK=-lpthread

# dir
CHANNEL_DIR=./channel
BASE_DIR=./base
CONFIG_DIR=./config
EPOLL_DIR=./epoll
EVENTLOOP_DIR=./eventloop
HTTP_DIR=./http
LOCK_DIR=./lock
LOG_DIR=./log
TIMER_DIR=./timer
TCP_DIR=./tcp

#	Source and Objects 
SOURCE:=$(wildcard ./*.cpp $(CHANNEL_DIR)/*.cpp\
    $(BASE_DIR)/*.cpp\
    $(CONFIG_DIR)/*.cpp\
    $(EPOLL_DIR)/*.cpp\
    $(EVENTLOOP_DIR)/*.cpp\
    $(HTTP_DIR)/*.cpp\
    $(LOCK_DIR)/*.cpp\
    $(LOG_DIR)/*.cpp\
    $(TIMER_DIR)/*.cpp\
    $(TCP_DIR)/*.cpp)  
OBJECTS:=$(patsubst %.cpp, %.o, $(SOURCE)) 

#	Target
MAIN_TARGET:=./bin/WebServer 
#	rule for Target 

$(MAIN_TARGET):$(OBJECTS)
	$(CC) $(CCFLAGS) -o $@ $^ $(CCLINK) 

Main.o: Main.cpp 
	$(CC) $(CCFLAGS) -c -o $@ $^
    
%.o:%.cpp 
	$(CC) $(CCFLAGS) -c -o $@ $<

.PHONY:
clean:
	rm $(OBJECTS) $(all) Main.o  bin/*  -rf
time:
	date