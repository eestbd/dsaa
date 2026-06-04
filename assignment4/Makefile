CPP=g++
CFLAG=-g
SRCS=$(wildcard *.cpp)
HDRS=$(wildcard *.h)
OBJS=$(SRCS:.cpp=.o)
TARGET=graph

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CPP) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CPP) $(CFLAG) -o $@ -c $<

clean:
	rm -f $(OBJS) $(TARGET)

