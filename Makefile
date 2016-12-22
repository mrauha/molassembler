.PHONY: all 
all:
	g++ -g -O2 -std=c++14 -Wall -Wpedantic -Wextra -otest -L/usr/lib/x86_64-linux-gnu/ tests.cpp -lboost_unit_test_framework
	./test --report_level=detailed

clang:
	clang++ -std=c++14 -Wall -Wpedantic -Wextra -fsyntax-only tests.cpp 
