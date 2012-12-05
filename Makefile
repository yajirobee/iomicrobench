CC = gcc
READBENCH = sequentialread randomread
READWAFBENCH = sequentialreadaf randomreadaf

all: $(READBENCH) $(READWAFBENCH) cleanobject

$(READBENCH) $(READWAFBENCH): % : %.o iomicrobench.o
	$(CC) -o $@ $^ -lpthread

cleanobject:
	/bin/rm -f $(addsuffix .o, $(READBENCH)) $(addsuffix .o, $(READWAFBENCH)) iomicrobench.o

clean:
	/bin/rm -f $(READBENCH) $(READWAFBENCH)

.PHONY: check-syntax

check-syntax:
	$(CC) -Wall -fsyntax-only $(CHK_SOURCES)

.c.o:
	$(CC) -c $<
