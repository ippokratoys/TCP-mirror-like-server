OBJS1 	= ContentServer.o
OBJS2 	= MirrorServer.o
OBJS3   = MirrorInitiator.o
SOURCE	= MirrorServer.c MirrorInitiator.c ContentServer.c
#HEADER  =
OUT1    = ContentServer
OUT2    = MirrorServer
OUT3    = MirrorInitiator
OUT = $(OUT1) $(OUT2) $(OUT3)
CC	= gcc
FLAGS   = -g -c -pthread
# -g option enables debugging mode
# -c flag generates object code for separate files


all: $(OUT1) $(OUT2) $(OUT3)

$(OUT1): $(OBJS1)
	$(CC) -pthread -g $(OBJS1) -o $(OUT1)

$(OUT2): $(OBJS2)
	$(CC) -pthread -g $(OBJS2) -o $(OUT2)

$(OUT3): $(OBJS3)
	$(CC) -pthread -g $(OBJS3) -o $(OUT3)

# create/compile the individual files >>separately<<
ContentServer.o:
	$(CC) $(FLAGS) ContentServer.c

MirrorServer.o:
	$(CC) $(FLAGS) MirrorServer.c

MirrorInitiator.o:
	$(CC) $(FLAGS) MirrorInitiator.c

clean:
	rm -f $(OBJS1) $(OBJS2) $(OBJS3) $(OUT1) $(OUT2) $(OUT3)
