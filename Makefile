CPPFILES = $(wildcard *.cpp)
TESTCPP = $(filter %_test.cpp,$(CPPFILES))
LIBCPP = $(filter-out %_test.cpp,$(CPPFILES))

CPPFLAGS = -I.
CXX = g++ -march=native -mtune=native -pipe -std=c++17
CXXFLAGS = -pedantic -Og -ggdb -Wall -Wextra -pthread
#CXXFLAGS = -pedantic -O0 -ggdb -Wall -Wextra -pthread -fprofile-arcs -ftest-coverage
#CXXFLAGS = -Ofast -ggdb -Wall -Wextra -pthread

%.o : %.cpp empty
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(<) -o $(@)

test: test_all
	./test_all

clean:
	rm -f *.o *.gcov *.gcda *.gcno

.PHONY: empty test doxygen

test_all: $(patsubst %.cpp,%.o,$(LIBCPP) $(TESTCPP))
	$(CXX) $(CXXFLAGS) $(^) -o $(@) -lboost_unit_test_framework

doxygen:
	if [ -d docs/doxygen ]; then \
	    rm -rf docs/doxygen/*; \
	fi
	mkdir -p docs/doxygen
	doxygen
