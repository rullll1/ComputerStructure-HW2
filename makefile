
cacheSim: cacheSim.cpp
	g++ -std=c++11 -o memory Memory.cpp -o cacheSim cacheSim.cpp

.PHONY: clean
clean:
	rm -f *.o
	rm -f cacheSim
