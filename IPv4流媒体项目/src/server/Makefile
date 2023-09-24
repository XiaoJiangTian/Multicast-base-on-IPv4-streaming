CFLAGS+=-I../include/  -pthread
LDFALGS+=-pthread

all:server

server:server.o thr_channel.o thr_list.o mytbf.o medialib.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFALGS)

clean:
	rm *.o server -rf