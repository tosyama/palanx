all: build/CMakeCache.txt
	cmake --build build
	# cd build/test && rm -f core && ./tester
lcov:
	# cd build && lcov -c -d . -b src -o all.info
	# cd build && lcov -e all.info */c2pa/src/* -o lcov.info
build/CMakeCache.txt:
	mkdir -p build
	cd build && cmake ..
clean:
	rm -r build


