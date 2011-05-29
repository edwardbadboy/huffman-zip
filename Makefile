EXES=huffman_zip huffman_zip_heap
OBJS=$(EXES:%=%.o)
CPP = g++
CFLAGS = -O2 -Wall -Wextra
MAKE=make

.PHONY: all clean test test_resource

all: dependency $(EXES)

include dependency

$(EXES): %: %.o
	$(CPP) -o $@ $(LDFLAGS) $^

%.o: %.cpp
	$(CPP) -c -o $@ $(CFLAGS) $<

dependency: $(OBJS:%.o=%.cpp)
	$(CPP) -MM $^ >$@

test: test_resource all
	$(MAKE) -C $< test

clean:
	rm -rf $(EXES) $(OBJS) dependency
