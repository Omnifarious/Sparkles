CPPFILES = $(wildcard *.cpp)
TESTCPP = $(filter %_test.cpp,$(CPPFILES))
LIBCPP = $(filter-out %_test.cpp,$(CPPFILES))

CPPFLAGS = -I.
CXX = g++ -std=c++0x -pedantic
#CXXFLAGS = -O0 -ggdb -Wall -Wextra -pthread -fprofile-arcs -ftest-coverage
CXXFLAGS = -O0 -ggdb -Wall -Wextra -pthread
#CXXFLAGS = -O3 -ggdb -Wall -Wextra -pthread

%.o : %.cpp empty
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(<) -o $(@)

test: test_all
	./test_all

clean:
	rm -f *.o *.gcov *.gcda *.gcno

.PHONY: empty test

test_all: $(patsubst %.cpp,%.o,$(LIBCPP) $(TESTCPP))
	$(CXX) $(CXXFLAGS) $(^) -o $(@) -lboost_unit_test_framework

