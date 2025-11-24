#!/bin/bash

# --- 配置部分 ---
QT_PATH="$HOME/Qt/6.9.2/gcc_64"

# 遇到错误立即停止脚本
set -e

echo "============================================="
echo "   开始 TransAI 自动化构建与安装脚本"
echo "   Qt 路径: $QT_PATH"
echo "============================================="

# 1. 检查 Qt 路径是否存在 (防呆检查)
if [ ! -d "$QT_PATH" ]; then
  echo "错误: 找不到 Qt 目录 -> $QT_PATH"
  echo "请检查路径是否正确安装。"
  exit 1
fi

# 2. 清理并创建 build 目录
if [ -d "build" ]; then
  echo "[1/4] 检测到 build 目录，正在清理..."
  rm -rf build
else
  echo "[1/4] build 目录不存在，准备创建..."
fi

mkdir build
cd build
echo "-> build 目录准备就绪"

# 3. 运行 CMake (核心修复：写入 RPATH)
echo "[2/4] 正在配置 CMake..."
cmake \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_INSTALL_PREFIX=/usr/local \
  -D CMAKE_PREFIX_PATH="$QT_PATH" \
  -D CMAKE_INSTALL_RPATH="$QT_PATH/lib" \
  -D CMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE \
  ..

# 4. 编译
echo "[3/4] 正在编译 (使用 $(nproc) 线程)..."
make -j$(nproc)

# 5. 安装
echo "[4/4] 正在安装 (需要 sudo 权限)..."
sudo make install

# 6. 刷新动态库缓存
echo "-> 刷新动态库缓存..."
sudo ldconfig

echo "============================================="
echo "   安装完成！"
echo "   现在你可以直接在终端输入 'TransAI' 运行。"
echo "============================================="
