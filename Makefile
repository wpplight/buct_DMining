.PHONY: init run clean help

# 默认目标
.DEFAULT_GOAL := help

# 构建目录
BUILD_DIR := build
BINARY := $(BUILD_DIR)/dig

# init: 创建 build 目录并运行 cmake 配置
init:
	@echo "正在初始化构建环境..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake ..
	@echo "初始化完成！"

# run: 编译并运行程序
run: $(BINARY)
	@echo "正在运行程序..."
	@$(BINARY)

# 编译二进制文件（如果不存在或源文件更新）
$(BINARY):
	@if [ ! -d "$(BUILD_DIR)" ]; then \
		echo "构建目录不存在，请先运行 'make init'"; \
		exit 1; \
	fi
	@echo "正在编译..."
	@cd $(BUILD_DIR) && make
	@echo "编译完成！"

# clean: 清理构建文件
clean:
	@echo "正在清理构建文件..."
	@rm -rf $(BUILD_DIR)
	@echo "清理完成！"

# help: 显示帮助信息
help:
	@echo "可用的 make 命令："
	@echo "  make init  - 创建 build 目录并运行 cmake 配置"
	@echo "  make run   - 编译并运行程序"
	@echo "  make clean - 清理构建文件"
	@echo "  make help  - 显示此帮助信息"

