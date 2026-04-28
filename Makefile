all: build/CMakeCache.txt
	cmake --build build
	find build -name "*.gcda" -delete 2>/dev/null; true
	@cd build && rm -f bin/core && bin/c2ast-tester
	@cd build && rm -f bin/core && bin/gen-ast-tester
	@cd build && rm -f bin/core && bin/sa-tester
	@cd build && rm -f bin/core && bin/codegen-tester
	@cd build && rm -f bin/core && bin/sa-unit-tester
	@cd build && rm -f bin/core && bin/codegen-unit-tester
	@cd build && rm -f bin/core && bin/build-mgr-tester

build/CMakeCache.txt:
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
clean:
	rm -r build
LCOV_FLAGS = --rc branch_coverage=1
LCOV_FILTER = --rc no_exception_branch=1
coverage: build-cov/CMakeCache.txt
	find build-cov -name "*.gcda" -delete 2>/dev/null; true
	cmake --build build-cov
	cd build-cov && bin/c2ast-tester
	cd build-cov && bin/gen-ast-tester
	cd build-cov && bin/sa-tester
	cd build-cov && bin/codegen-tester
	cd build-cov && bin/build-mgr-tester
	cd build-cov && lcov -c -d . -b src -o all.info $(LCOV_FLAGS) --ignore-errors mismatch
	cd build-cov && lcov -e all.info '*/palanx/src/*' -o lcov.info $(LCOV_FLAGS) $(LCOV_FILTER)
	cd build-cov && lcov -r lcov.info '*/PlnLexer.cpp' '*/PlnParser.cpp' '*/PlnParser.h' '*/location.hh' -o lcov.info $(LCOV_FLAGS) $(LCOV_FILTER)
build-cov/CMakeCache.txt:
	mkdir -p build-cov
	cd build-cov && cmake -DOUTPUT_COVERAGE=ON ..
coverage-reset:
	rm -rf build-cov
coverage-codegen: build-cov/CMakeCache.txt
	find build-cov -name "*.gcda" -delete 2>/dev/null; true
	cmake --build build-cov
	cd build-cov && bin/codegen-tester
	cd build-cov && bin/build-mgr-tester
	cd build-cov && lcov -c -d . -b src -o all.info $(LCOV_FLAGS) --ignore-errors mismatch
	cd build-cov && lcov -e all.info '*/palanx/src/codegen/*' -o lcov.info $(LCOV_FLAGS) $(LCOV_FILTER)
	cd build-cov && lcov -r lcov.info '*/PlnParser.cpp' '*/PlnParser.h' '*/location.hh' -o lcov.info $(LCOV_FLAGS) $(LCOV_FILTER)
coverage-sa: build-cov/CMakeCache.txt
	find build-cov -name "*.gcda" -delete 2>/dev/null; true
	cmake --build build-cov
	cd build-cov && bin/sa-tester
	cd build-cov && bin/sa-unit-tester
	cd build-cov && lcov -c -d . -b src -o all.info $(LCOV_FLAGS) --ignore-errors mismatch
	cd build-cov && lcov -e all.info '*/palanx/src/semantic-anlyzr/*' -o lcov.info $(LCOV_FLAGS) $(LCOV_FILTER)


