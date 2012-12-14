CC = gcc
CFLAGS = -I ../../
READWAFBENCH = sequentialreadaf randomreadaf

all: $(READWAFBENCH) cleanobject

$(READWAFBENCH): % : %.o iomicrobench.o
	$(CC) -o $@ $^ -lpthread

cleanobject:
	/bin/rm -f $(addsuffix .o, $(READWAFBENCH)) iomicrobench.o

clean:
	/bin/rm -f $(READWAFBENCH)

.PHONY: check-syntax clean

check-syntax:
	$(CC) -Wall -fsyntax-only $(CHK_SOURCES)

.c.o:
	$(CC) $(CFLAGS) -c $<

