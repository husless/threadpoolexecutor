CXX=clang++
CXXFLAGS=-std=c++17 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic
LINKFLAGS=-pthread -latomic
SOURCES=$(wildcard *.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))
PROGRAM=a.out

all:$(PROGRAM)

$(PROGRAM):$(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(PROGRAM) $(OBJECTS) $(LINKFLAGS)

.PHONY: clean distclean

clean:
	-rm -f $(OBJECTS)

distclean:
	-rm -f $(OBJECTS) $(PROGRAM)
