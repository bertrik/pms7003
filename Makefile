CXXFLAGS = -W -Wall -O2

all: test

clean:
	rm -f pms7003_test

test: pms7003_test
	./pms7003_test

