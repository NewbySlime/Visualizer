main: compile link
	

compile:
	g++ -c ByteIterator.cpp initquit.cpp main.cpp socket.cpp soundget.cpp -g

link:
	g++ -o main.exe ByteIterator.o initquit.o main.o socket.o soundget.o -lole32 -lksuser -lws2_32 -lmswsock -ladvapi32 -lkernel32 -lcomctl32