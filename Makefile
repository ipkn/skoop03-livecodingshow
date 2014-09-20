all:
	#g++ -Wall -O2 -std=c++1y -o server main.cpp -pthread -lboost_system -lboost_thread
	clang++ -Wall -O2 -std=c++1y -o server main.cpp MurmurHash3.cpp -pthread -lboost_system -lboost_thread
