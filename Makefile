all: runtime

runtime:
	echo [+] runtime block
	g++ -w -fopenmp src/main.cpp -o test/test
	time ./test/test
	echo [+] runtime block Complete
	
clean:
	rm -r -f test/test