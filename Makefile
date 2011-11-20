CPPFILES = $(wildcard *.cpp)
TESTCPP = $(filter %_test.cpp,$(CPPFILES))
LIBCPP = $(filter-out %_test.cpp,$(CPPFILES))

CPPFLAGS = -I.
CXX = g++ -std=c++0x -pedantic
CXXFLAGS = -O0 -ggdb -Wall -Wextra

%.o : %.cpp empty
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(<) -o $(@)

test: test_all
	./test_all

.PHONY: empty test

test_all: $(patsubst %.cpp,%.o,$(LIBCPP) $(TESTCPP))
	$(CXX) $(CXXFLAGS) $(^) -o $(@) -lboost_unit_test_framework

