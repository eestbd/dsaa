CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
TARGET = leaderboard

SRCS = main.cpp Manager.cpp
HDRS = Leaderboard.h Manager.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
