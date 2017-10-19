all: rowhammer
clean:
	rm -f rowhammer
rowhammer: rowhammer.cc
	g++ -std=c++11 -O3 -o $@ $@.cc

