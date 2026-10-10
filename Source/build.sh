#!/bin/sh
# ============================================================================
# ContourArx 一键构建脚本 (VC6 + ObjectARX R15 SDK, Win32)
# ----------------------------------------------------------------------------
# 配置完全对应 Source\ContourArx.dsp 中 "ContourArx - Win32 Release" 的
# CPP / RSC / LINK32 三段设置, 用于在 IDE 之外验证/重建 .arx。
#
# 用法:  sh build.sh              -> 生成 Source\Release\ContourArx.arx
#        VCROOT=... ARXSDK=... sh build.sh   -> 覆盖工具链/SDK 位置
#
# 依赖环境变量:
#   VCROOT   VC6 安装根目录        (默认 D:\SoftwareDevelop\Microsoft Visual Studio-zbh)
#   ARXSDK   ObjectARX R15 SDK    (需含 inc\ 与 lib\)
# ============================================================================
set -u

# --- 允许用 MSYS 风格路径传入; 同时准备两种形式 ---
#     *_M (MSYS 风格 /d/...) : 用于在 shell 里调用可执行文件、定位目录
#     *_W (Windows 风格 D:\...): 用于 INCLUDE / LIB 环境变量
to_msys() { echo "$1" | sed 's#\\#/#g' | sed 's#^\([a-zA-Z]\):#/\1#'; }
to_win()  { echo "$1" | sed 's#^/\([a-zA-Z]\)/#\1:/#' | sed 's#/#\\#g'; }

VCROOT_IN="${VCROOT:-/d/SoftwareDevelop/Microsoft Visual Studio-zbh}"
ARXSDK_IN="${ARXSDK:-/d/SoftwareDevelop/四院科研成果/CASDVSS/TSYRSD2003}"

VCROOT_M="$(to_msys "$VCROOT_IN")"
VCROOT_W="$(to_win  "$VCROOT_M")"
ARXSDK_M="$(to_msys "$ARXSDK_IN")"
ARXSDK_W="$(to_win  "$ARXSDK_M")"

SRC_M="$(cd "$(dirname "$0")" && pwd)"            # MSYS 风格
SRC_W="$(cd "$(dirname "$0")" && pwd -W)"         # Windows 风格

VC="$VCROOT_M/VC98"
VCCOMMON="$VCROOT_M/Common/MSDev98/Bin"   # RC.EXE / DUMPBIN.EXE 在这里, 不在 VC98\Bin

export PATH="$VC/Bin:$VCCOMMON:$PATH"
export INCLUDE="$VCROOT_W\\VC98\\Include;$VCROOT_W\\VC98\\MFC\\Include;$VCROOT_W\\VC98\\ATL\\Include;$ARXSDK_W\\inc;$SRC_W;$SRC_W\\GridControlSource\\GridCtrl_src;$SRC_W\\GridControlSource\\NewCellTypes"
export LIB="$VCROOT_W\\VC98\\Lib;$VCROOT_W\\VC98\\MFC\\Lib;$ARXSDK_W\\lib"

CL="$VC/Bin/CL.EXE"
RC="$VCCOMMON/RC.EXE"
LINK="$VC/Bin/LINK.EXE"

# 与 .dsp 的 "# ADD CPP ..." 完全一致
CPPFLAGS='/nologo /MD /W3 /Gi /GX /Od /D NDEBUG /D RADPACK /D ACRXAPP /D STRICT /D WIN32 /D _WINDOWS /D _WINDLL /D _AFXDLL /c'
RSCFLAGS='/l 0x407 /d NDEBUG /d _AFXDLL'

cd "$(dirname "$0")" || exit 1
mkdir -p Release _build_obj

# 编译器输出是 GBK, 统一转成 UTF-8 方便阅读
show() { iconv -f GBK -t UTF-8 "$1" 2>/dev/null || cat "$1"; }

echo "=== [1/4] 生成预编译头 (stdafx.cpp) ==="
MSYS_NO_PATHCONV=1 "$CL" $CPPFLAGS /Yc"stdafx.h" \
    /Fp"Release\\ContourArx.pch" /Fo"_build_obj\\stdafx.obj" stdafx.cpp > /tmp/_c.log 2>&1
if [ ! -f "Release/ContourArx.pch" ]; then
  echo "  预编译头生成失败:"; show /tmp/_c.log | head -20; exit 1
fi
echo "  OK"

echo "=== [2/4] 编译全部源文件 ==="
FAIL=0; TOTAL=0
for f in *.cpp *.Cpp \
         GridControlSource/GridCtrl_src/*.cpp \
         GridControlSource/NewCellTypes/*.cpp ; do
  case "$f" in
    stdafx.cpp) continue ;;
    # 原作者遗留的未完成文件, 含 "return TRUE/FALSE;" 非法语句, 不参与构建
    ProtectApp.cpp) continue ;;
  esac
  [ -f "$f" ] || continue
  flat=$(echo "$f" | tr 'A-Z' 'a-z' | sed 's#[\\/]#_#g; s/\.cpp$/.obj/')
  if ! MSYS_NO_PATHCONV=1 "$CL" $CPPFLAGS /Yu"stdafx.h" \
        /Fp"Release\\ContourArx.pch" /Fo"_build_obj\\$flat" "$f" > /tmp/_c.log 2>&1 ; then
     echo "  [失败] $f"
     show /tmp/_c.log | grep -i 'error' | head -3
     FAIL=$((FAIL+1))
  fi
  TOTAL=$((TOTAL+1))
done
echo "  共 $TOTAL 个源文件, 失败 $FAIL 个"
[ "$FAIL" -eq 0 ] || exit 1

echo "=== [3/4] 编译资源 ==="
MSYS_NO_PATHCONV=1 "$RC" $RSCFLAGS /fo"Release\\ContourArx.res" ContourArx.rc > /tmp/_c.log 2>&1
if [ ! -f "Release/ContourArx.res" ]; then
  echo "  资源编译失败:"; show /tmp/_c.log | head -10; exit 1
fi
echo "  OK"

echo "=== [4/4] 链接 ==="
MSYS_NO_PATHCONV=1 "$LINK" /nologo /base:"0x1c000000" \
  /subsystem:windows /dll /incremental:yes /machine:I386 /nodefaultlib:"libc.lib" \
  /out:"Release\\ContourArx.arx" \
  _build_obj/*.obj "Release\\ContourArx.res" /DEF:ContourArx.def \
  rxapi.lib acrx15.lib acdb15.lib acutil15.lib acge15.lib acgiapi.lib acad.lib acedapi.lib \
  > /tmp/_link.log 2>&1
echo "  链接退出码: $?"
UNRES=$(grep -c 'unresolved external symbol' /tmp/_link.log 2>/dev/null)
echo "  未解析符号数: ${UNRES:-0}"
show /tmp/_link.log | grep -viE 'unresolved external symbol|warning|Creating library' | grep -v '^$' | head -10

echo
echo "=== 结果 ==="
if ls -la Release/ContourArx.arx 2>/dev/null; then echo "构建成功"; else echo "构建失败"; exit 1; fi
