main: compile link
	

compile:
	g++ -c initquit.cpp main.cpp socket.cpp soundget.cpp data_encryption.cpp -g

link:
	g++ -o Soundget.exe initquit.o main.o socket.o soundget.o data_encryption.o -lole32 -lksuser -lws2_32 -lmswsock -ladvapi32 -lkernel32 -lcomctl32 -lwinmm