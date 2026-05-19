#!/usr/bin/env bash

# TransAI 无 root 构建与用户级安装脚本
# 默认安装到: $HOME/.local/bin/TransAI
# 可覆盖变量示例:
#   QT_PATH=$HOME/Qt/6.9.2/gcc_64 INSTALL_PREFIX=$HOME/.local ./build_install.sh

set -euo pipefail

INSTALL_PREFIX="${INSTALL_PREFIX:-$HOME/.local}"
INSTALL_BIN_DIR="$INSTALL_PREFIX/bin"
APP_PATH="$INSTALL_BIN_DIR/TransAI"
BUILD_DIR="build"
BUILD_JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"

# --- Qt 检测逻辑 ---
detect_qt_path() {
    # 0. 优先使用用户显式指定的 QT_PATH
    if [ -n "${QT_PATH:-}" ]; then
        if [ -d "$QT_PATH/lib/cmake/Qt6" ] || [ -d "$QT_PATH/lib/cmake/qt6" ]; then
            echo "[配置] 使用环境变量 QT_PATH: $QT_PATH" >&2
            echo "$QT_PATH"
            return 0
        fi
        echo "[警告] QT_PATH 无效，继续自动检测: $QT_PATH" >&2
    fi

    # 1. 优先检查系统 Qt6/qmake6
    local qmake_cmd qt_prefix
    for qmake_cmd in qmake6 qmake; do
        if command -v "$qmake_cmd" >/dev/null 2>&1; then
            if qt_prefix=$("$qmake_cmd" -query QT_INSTALL_PREFIX 2>/dev/null); then
                if [ -d "$qt_prefix/lib/cmake/Qt6" ] || [ -d "$qt_prefix/lib/cmake/qt6" ]; then
                    echo "[自动检测] 发现系统 Qt6: $qt_prefix" >&2
                    echo "$qt_prefix"
                    return 0
                fi
            fi
        fi
    done

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

    local qt_path
    for qt_path in "${user_qt_paths[@]}"; do
        if [ -d "$qt_path/lib/cmake/Qt6" ] || [ -d "$qt_path/lib/cmake/qt6" ]; then
            echo "[自动检测] 发现本地 Qt: $qt_path" >&2
            echo "$qt_path"
            return 0
        fi
    done

    # 3. 检查 CMake 系统路径
    if [ -d "/usr/lib/cmake/Qt6" ] || [ -d "/usr/lib/x86_64-linux-gnu/cmake/Qt6" ]; then
        echo "[自动检测] 发现系统 CMake Qt6: /usr" >&2
        echo "/usr"
        return 0
    fi

    return 1
}

QT_PATH_DETECTED=$(detect_qt_path)
QT_PATH="$QT_PATH_DETECTED"

if [ -z "$QT_PATH" ]; then
    echo "============================================="
    echo "   错误: 未找到 Qt6 安装"
    echo "============================================="
    echo "请选择以下方案之一:"
    echo "  1. 安装系统 Qt6: sudo apt install qt6-base-dev qt6-declarative-dev"
    echo "  2. 下载 Qt6: https://www.qt.io/download-qt-installer"
    echo "  3. 设置环境变量: QT_PATH=\$HOME/Qt/6.9.2/gcc_64 ./build_install.sh"
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "错误: 未找到 cmake，请先安装 CMake。"
    exit 1
fi

if ! command -v make >/dev/null 2>&1; then
    echo "错误: 未找到 make，请先安装构建工具。"
    exit 1
fi

echo "============================================="
echo "   开始 TransAI 用户级构建与安装脚本"
echo "   Qt 路径: $QT_PATH"
echo "   安装前缀: $INSTALL_PREFIX"
echo "   程序路径: $APP_PATH"
echo "============================================="

# 1. 清理 build 目录，避免沿用旧的 /usr/local 安装前缀
if [ -d "$BUILD_DIR" ]; then
    echo "[1/4] 检测到 $BUILD_DIR 目录，正在清理..."
    rm -rf "$BUILD_DIR"
else
    echo "[1/4] $BUILD_DIR 目录不存在，准备创建..."
fi

# 2. 运行 CMake：安装到用户目录，并写入运行时库搜索路径，无需 ldconfig
#    $ORIGIN/../lib 用于查找安装在 $INSTALL_PREFIX/lib 下的本地库；$QT_PATH/lib 用于查找 Qt 库。
echo "[2/4] 正在配置 CMake..."
cmake -S . -B "$BUILD_DIR" \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -D CMAKE_PREFIX_PATH="$QT_PATH" \
    -D QT_DEFAULT_MAJOR_VERSION=6 \
    -D CMAKE_INSTALL_RPATH="\$ORIGIN/../lib;$QT_PATH/lib" \
    -D CMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE

# 3. 编译
echo "[3/4] 正在编译 (使用 $BUILD_JOBS 线程)..."
cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"

# 4. 用户级安装，不需要 sudo/root
echo "[4/4] 正在安装到用户目录 (无需 sudo/root)..."
cmake --install "$BUILD_DIR"

if [ ! -x "$APP_PATH" ]; then
    echo "错误: 未找到安装后的可执行文件: $APP_PATH"
    exit 1
fi

echo "============================================="
echo "   安装完成！"
echo "   可执行文件: $APP_PATH"
echo "============================================="

case ":$PATH:" in
    *":$INSTALL_BIN_DIR:"*)
        echo "现在可以直接运行: TransAI"
        ;;
    *)
        echo "提示: $INSTALL_BIN_DIR 不在 PATH 中。"
        echo "你可以直接运行: $APP_PATH"
        echo "或将下面这一行加入 ~/.bashrc / ~/.zshrc 后重新打开终端:"
        echo "  export PATH=\"$INSTALL_BIN_DIR:\$PATH\""
        ;;
esac
