EXECUTABLE = http_server
OBJECTS = main.o message.o
HEADERS = http_server.h
WARNINGS = -Wall -Wextra -Werror
CFLAGS = -std=gnu23 -g

$(EXECUTABLE) : $(OBJECTS)
	$(CC) $^ -o $@

%.o : %.c $(HEADERS)
	$(CC) $< -o $@ -c $(CFLAGS) $(WARNINGS)

clean :
	rm -f $(EXECUTABLE) $(OBJECTS)
