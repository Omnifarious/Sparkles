CPPFILES = $(wildcard *.cpp)
TESTCPP = $(filter %_test.cpp,$(CPPFILES))
LIBCPP = $(filter-out %_test.cpp,$(CPPFILES))

CPPFLAGS = -I.
CXX = g++ -march=native -mtune=native -pipe -std=c++0x
#CXXFLAGS = -pedantic -O0 -ggdb -Wall -Wextra -pthread -fprofile-arcs -ftest-coverage
CXXFLAGS = -pedantic -O0 -ggdb -Wall -Wextra -pthread
#CXXFLAGS = -Ofast -ggdb -Wall -Wextra -pthread

%.o : %.cpp empty
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(<) -o $(@)

test: test_all
	./test_all

clean:
	rm -f *.o *.gcov *.gcda *.gcno

.PHONY: empty test

test_all: $(patsubst %.cpp,%.o,$(LIBCPP) $(TESTCPP))
	$(CXX) $(CXXFLAGS) $(^) -o $(@) -lboost_unit_test_framework

