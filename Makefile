all: build/CMakeCache.txt
	cmake --build build
	cd build && rm -f bin/core && bin/gen-ast-tester
build/CMakeCache.txt:
	mkdir -p build
	cd build && cmake ..
clean:
	rm -r build
coverage:
	rm -rf build
	mkdir build
	cd build && cmake -DOUTPUT_COVERAGE=ON ..
	cmake --build build
	cd build && bin/gen-ast-tester
	cd build && lcov -c -d . -b src -o all.info
	cd build && lcov -e all.info */palanx/src/* -o lcov.info

