
BUILD_DIR := build
all:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..
	make -j -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean