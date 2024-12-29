CC	= g++ -g3
CFLAGS  = -g3
TARGET1 = user_proc
TARGET2 = oss 

OBJS1	= user_proc.o
OBJS2	= oss.o

all:	$(TARGET1) $(TARGET2)

$(TARGET1):	$(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1)

$(TARGET2):	$(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2)

# Explicit dependencies for user_proc.o
user_proc.o:	user_proc.cpp descriptor.h
	$(CC) $(CFLAGS) -c user_proc.cpp

# Explicit dependencies for oss.o
oss.o:	oss.cpp descriptor.h
	$(CC) $(CFLAGS) -c oss.cpp

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2)