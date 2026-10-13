CXX = g++
CXXFLAGS = -Wall -Wextra -I. -Itests -Imock_headers -std=c++11

TEST_TARGET = run_tests
SRCS = tests/mqtt_callback_impl.cpp tests/test_mqtt_callback.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TEST_TARGET)

$(TEST_TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(OBJS) $(TEST_TARGET)

.PHONY: all test clean
