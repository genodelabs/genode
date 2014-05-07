TARGET   = httpsrv_connect
SRC      = main.c net.c

CFLAGS   = -Wall -Wextra -Wno-unused
ifeq ($(time),1)
CC_DEF  += -DUGLY_MEASURE_TIME
endif
LDFLAGS  = -pthread

OBJS    = $(SRC:.c=.o)

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

.c.o:
	$(CC) -c $(CFLAGS) $(CC_DEF) $^ -o $@

clean:
	-rm -rf ./$(TARGET) ./*.o
