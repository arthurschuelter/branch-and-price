BUILD_DIR ?= build
DEBUG_BUILD_DIR ?= build-debug

# Parâmetros da execução (-f -s -C -t -h -o)
F ?= tests/instance_zero.txt
S ?= 0
C ?= 10
T ?= 60
H ?= 10
O ?= results/instance_zero.txt

.PHONY: all configure run debug clean

all: configure
	cmake --build $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

run: all
	@mkdir -p $(dir $(O))
	./$(BUILD_DIR)/bp -f $(F) -s $(S) -C $(C) -t $(T) -h $(H) -o $(O)

debug:
	cmake -S . -B $(DEBUG_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG="-g -O0 -fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS_DEBUG="-fsanitize=address,undefined"
	cmake --build $(DEBUG_BUILD_DIR)

clean:
	cmake -E rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR)
