CC = gcc
READBENCH = sequentialread randomread

all: $(READBENCH)

$(READBENCH) : % : %.o util.o
	$(CC) -o $@ $^ -lpthread -lrt

cleanobject:
	/bin/rm -f $(addsuffix .o, $(READBENCH)) util.o

clean: cleanobject
	/bin/rm -f $(READBENCH)

.PHONY: check-syntax

check-syntax:
	$(CC) -Wall -fsyntax-only $(CHK_SOURCES)

.c.o:
	$(CC) -c $<
