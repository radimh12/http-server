EXECUTABLE = http_server
OBJECTS = main.o
WARNINGS = -Wall -Wextra -Werror
CFLAGS = -std=gnu23 -g

$(EXECUTABLE) : $(OBJECTS)
	$(CC) $^ -o $@

%.o : %.c
	$(CC) $< -o $@ -c $(CFLAGS) $(WARNINGS)

clean :
	rm -f $(EXECUTABLE) $(OBJECTS)
