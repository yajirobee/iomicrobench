CC = gcc
READBENCH = sequentialread randomread

all: $(READBENCH) cleanobject

$(READBENCH) : % : %.o iomicrobench.o
	$(CC) -o $@ $^ -lpthread

cleanobject:
	/bin/rm -f $(addsuffix .o, $(READBENCH)) iomicrobench.o

clean:
	/bin/rm -f $(READBENCH)

.PHONY: check-syntax

check-syntax:
	$(CC) -Wall -fsyntax-only $(CHK_SOURCES)

.c.o:
	$(CC) -c $<
