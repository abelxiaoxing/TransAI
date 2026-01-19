#!/bin/bash

# --- Qt 检测逻辑 ---
detect_qt_path() {
    # 1. 优先检查系统 Qt6 (通过包管理器安装)
    if command -v qmake &> /dev/null; then
        local qt_prefix=$(qmake -query QT_INSTALL_PREFIX 2>/dev/null)
        if [ -d "$qt_prefix/lib/cmake/Qt6" ] || [ -d "$qt_prefix/lib/cmake/qt6" ]; then
            echo "[自动检测] 发现系统 Qt6: $qt_prefix"
            echo "$qt_prefix"
            return 0
        fi
    fi

    # 2. 检查用户本地 Qt 安装
    local user_qt_paths=(
        "$HOME/Qt/6.9.2/gcc_64"
        "$HOME/Qt/6.9.1/gcc_64"
        "$HOME/Qt/6.8.2/gcc_64"
        "$HOME/Qt/6.8.1/gcc_64"
        "$HOME/Qt/6.8.0/gcc_64"
        "$HOME/Qt/6.7.3/gcc_64"
        "$HOME/Qt/6.7.2/gcc_64"
        "$HOME/Qt/6.7.1/gcc_64"
        "$HOME/Qt/6.7.0/gcc_64"
        "$HOME/Qt/6.6.2/gcc_64"
        "$HOME/Qt/6.6.1/gcc_64"
        "$HOME/Qt/6.6.0/gcc_64"
    )

    for qt_path in "${user_qt_paths[@]}"; do
        if [ -d "$qt_path" ]; then
            echo "[自动检测] 发现本地 Qt: $qt_path"
            echo "$qt_path"
            return 0
        fi
    done

    # 3. 检查 CMake 系统路径
    if [ -d "/usr/lib/cmake/Qt6" ]; then
        echo "[自动检测] 发现系统 CMake Qt6: /usr/lib"
        echo "/usr"
        return 0
    fi

    return 1
}

QT_PATH=$(detect_qt_path)
if [ -z "$QT_PATH" ]; then
    echo "============================================="
    echo "   错误: 未找到 Qt6 安装"
    echo "============================================="
    echo "请选择以下方案之一:"
    echo "  1. 安装系统 Qt6: sudo apt install qt6-base-dev"
    echo "  2. 下载 Qt6: https://www.qt.io/download-qt-installer"
    echo "  3. 设置环境变量: export QT_PATH=\$HOME/Qt/6.9.2/gcc_64"
    exit 1
fi

# 遇到错误立即停止脚本
set -e

echo "============================================="
echo "   开始 TransAI 自动化构建与安装脚本"
echo "   Qt 路径: $QT_PATH"
echo "============================================="

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
