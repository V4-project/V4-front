.PHONY: all build test clean format format-check release size

# Default target
all: build test

# Build (fetch V4 from Git by default)
build:
	@echo "🔨 Building V4-front..."
	@cmake -B build -DCMAKE_BUILD_TYPE=Debug -DV4_FETCH=ON
	@cmake --build build -j

# Build with local V4 include directory
build-local:
	@echo "🔨 Building V4-front with local V4..."
	@cmake -B build -DCMAKE_BUILD_TYPE=Debug -DV4_INCLUDE_DIR=$(V4_INCLUDE_DIR)
	@cmake --build build -j

# Release build
release:
	@echo "🚀 Building release..."
	@cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DV4_FETCH=ON
	@cmake --build build-release -j

# Release build with local V4
release-local:
	@echo "🚀 Building release with local V4..."
	@cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DV4_INCLUDE_DIR=$(V4_INCLUDE_DIR)
	@cmake --build build-release -j

# Run tests
test: build
	@echo "🧪 Running tests..."
	@cd build && ctest --output-on-failure

# Clean
clean:
	@echo "🧹 Cleaning..."
	@rm -rf build build-release build-debug build-asan build-ubsan

# Apply formatting
format:
	@echo "✨ Formatting C/C++ code..."
	@find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) \
		-not -path "*/vendor/*" -exec clang-format -i {} \;
	@echo "✨ Formatting CMake files..."
	@find . -name 'CMakeLists.txt' -o -name '*.cmake' | xargs cmake-format -i
	@echo "✅ Formatting complete!"

# Format check
format-check:
	@echo "🔍 Checking C/C++ formatting..."
	@find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) \
		-not -path "*/vendor/*" | xargs clang-format --dry-run --Werror || \
		(echo "❌ C/C++ formatting check failed." && exit 1)
	@echo "🔍 Checking CMake formatting..."
	@find . -name 'CMakeLists.txt' -o -name '*.cmake' | xargs cmake-format --check || \
		(echo "❌ CMake formatting check failed." && exit 1)
	@echo "✅ All formatting checks passed!"

# Sanitizer build
asan: clean
	@echo "🛡️  Building with AddressSanitizer..."
	@cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DV4_FETCH=ON \
		-DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
	@cmake --build build-asan -j
	@echo "🧪 Running tests with AddressSanitizer..."
	@cd build-asan && ctest --output-on-failure

ubsan: clean
	@echo "🛡️  Building with UndefinedBehaviorSanitizer..."
	@cmake -B build-ubsan -DCMAKE_BUILD_TYPE=Debug -DV4_FETCH=ON \
		-DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g"
	@cmake --build build-ubsan -j
	@echo "🧪 Running tests with UndefinedBehaviorSanitizer..."
	@cd build-ubsan && ctest --output-on-failure

# Build release and show stripped binary sizes
size:
	@echo "📦 Building release with size optimization..."
	@cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DV4_FETCH=ON
	@cmake --build build-release -j
	@echo ""
	@echo "📊 Stripping and measuring binary sizes..."
	@echo ""
	@echo "=== Library ==="
	@if [ -f build-release/libv4front.a ]; then \
		cp build-release/libv4front.a build-release/libv4front.stripped.a && \
		strip --strip-debug build-release/libv4front.stripped.a && \
		ls -lh build-release/libv4front.a build-release/libv4front.stripped.a | awk '{print $$9 ": " $$5}'; \
	fi
	@echo ""
	@echo "=== Test Executables (stripped) ==="
	@for binary in build-release/test_*; do \
		if [ -f "$$binary" ] && [ -x "$$binary" ]; then \
			cp "$$binary" "$${binary}.stripped" && \
			strip "$${binary}.stripped" && \
			echo "$$(basename $$binary): $$(ls -lh $${binary}.stripped | awk '{print $$5}')"; \
		fi \
	done
	@echo ""
	@echo "✅ Size measurement complete!"

# Help
help:
	@echo "V4-front Makefile targets:"
	@echo ""
	@echo "  make / make all      - Build and test (fetches V4 from Git)"
	@echo "  make build           - Build only (fetches V4 from Git)"
	@echo "  make build-local     - Build with local V4 (set V4_INCLUDE_DIR=/path/to/V4/include)"
	@echo "  make release         - Release build (fetches V4 from Git)"
	@echo "  make release-local   - Release build with local V4"
	@echo "  make test            - Run tests"
	@echo "  make clean           - Remove build directories"
	@echo "  make format          - Format code with clang-format and cmake-format"
	@echo "  make format-check    - Check formatting without modifying files"
	@echo "  make asan            - Build and test with AddressSanitizer"
	@echo "  make ubsan           - Build and test with UndefinedBehaviorSanitizer"
	@echo "  make size            - Build release and show stripped binary sizes"
	@echo "  make help            - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                                          # Default: build and test"
	@echo "  make build-local V4_INCLUDE_DIR=../V4/include # Use local V4"
	@echo "  make release                                  # Release build"