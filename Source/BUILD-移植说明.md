# ContourArx 移植编译说明（VC6 + ObjectARX R15 SDK）

原仓库 `CivilRoadDesignUtilArx` 是 2019 年的老工程，依赖若干**未随源码发布的私有组件**，
直接打开会报：

```
fatal error C1083: Cannot open include file: 'LicenseAll.h': No such file or directory
```

修完编译期问题后又会撞上链接期：

```
LINK : fatal error LNK1104: cannot open file "License.lib"
```

本目录下的改动用于**最小化**修复这两个问题，**使工程可以完整编译并成功产出 `.arx`**。

**当前状态：已验证通过。**
```
=== [1/4] 生成预编译头 (stdafx.cpp) ===   OK
=== [2/4] 编译全部源文件 ===              共 69 个源文件, 失败 0 个
=== [3/4] 编译资源 ===                    OK
=== [4/4] 链接 ===                       链接退出码: 0 / 未解析符号数: 0
=== 结果 ===                             Release/ContourArx.arx  (749,568 字节)
```

一键重建：`sh build.sh`（在 `Source` 目录下执行）。

---

## 一、改动清单

| 文件 | 改动 |
|---|---|
| `LicenseAll.h` | **新增**。补回缺失的授权库接口声明 + 实现（详见第二、三节）。**文件内容为纯 ASCII 英文注释**（初版写成 GBK 中文，在 GitHub 上显示乱码，已更正；见第五节「源文件编码约定」） |
| `ContourArx.cpp` | 补 `_hdllInstance` 的定义（原工程只有 `extern` 声明） |
| `ContourArx.dsp` | ① 登记 `LicenseAll.h`；② 两条链接行去掉 `License.lib`、去掉 `/entry:"DllEntryPoint@12"`、`Libacge.lib`→`acge15.lib`、`Acgiapi.lib`→`acgiapi.lib`、补上 `acrx15.lib acdb15.lib acutil15.lib`、删掉指向不存在的 `LicenseKeyGenerator` 的 `/I` 与 `/libpath` |
| `CoOrdMarker.cpp`<br>`DwgParamObj.cpp`<br>`HAlignment.cpp`<br>`NEAnnotationEntity.cpp`<br>`TINData.cpp`<br>`TINEntity.cpp`<br>`VAlignment.cpp` | 各 1 处：`ACRX_DXF_DEFINE_MEMBERS(` → `ACRX_DXF_DEFINE_MEMBERS5(` |
| `ContourArx.def` | 行尾 LF → CRLF（VC6 的 linker 对这个文件挑格式） |
| `build.sh` | **新增**。命令行一键构建脚本，配置与 `.dsp` 完全一致 |

**`stdafx.h` 未改动**：仍是 `#include <LicenseAll.h>`，与原始工程完全一致。

> ⚠️ 已确认源码里**没有任何** `#pragma comment(lib, "...")`，所以 obj 不会自动请求
> ObjectARX 的库，库必须显式写进链接命令行（这也是 `.dsp` 链接行必须补齐 `acdb15.lib` 等的原因）。

---

## 二、C1083 的根因

`stdafx.h:71` 有 `#include <LicenseAll.h>`，但该头文件属于厂商私有授权库：

```
..\..\LicenseKeyGenerator\License\
```

`LicenseKeyGenerator` 目录**从未进入 Git 仓库**（作者的私有产品），因此任何克隆者都会
缺这个头文件。同时缺失的还有：

- 头文件：`LicenseAll.h`（`CLicenseManager` / `CApplcationProtector` /
  `CProtectionInterfaceBase` / `CProtectionDebugging` 的声明）
- 库文件：`License.lib`（链接期需要）

### 处理方式

`LicenseAll.h` 用最小的桩实现补齐上述符号，**全部内联、不产生任何需要外部库的引用**，
从而在编译期和链接期同时消除对 `License.lib` 的依赖：

| 类/函数 | 桩实现行为 |
|---|---|
| `CProtectionInterfaceBase` | 空基类（虚析构） |
| `CLicenseManager::ReadLiceseInfo` | 读文件到 `new char[]`（调用方 `delete[]`） |
| `CLicenseManager::CheckValidity` | 仅判非空（无签名私钥，无法做真验签） |
| `CApplcationProtector::Initialize` | 恒返回 `FaultType::Success` |
| `CApplcationProtector::ResetLicenseRelatedData` | 空操作 |
| `CProtectionDebugging::Start/EndDebugLog` | 空操作 |
| `CApplcationProtector::GetDebugLogFileName` | 返回 `"ContourARX.log"` |

> **注销规模的说明**：整个工程里真正依赖 `License.lib` 的只有
> `CApplcationProtector` / `CLicenseManager` / `CProtectionInterfaceBase` /
> `CProtectionDebugging` **这 4 个类**（源码引用点集中在 `ContourApp.Cpp`、
> `SystemRegistryManager.cpp`、`LicenseKeySelectionDlg.cpp`）。
> 因此只桩掉这 4 个类即可 —— **没有改动任何业务代码**，
> `ContourArx` / `TIN` / `Alignment` / `NEAnnotation` 等实体的实现全部原样保留。

> ⚠️ **这是移除了授权校验的编译桩**：编译出的 ARX 不再需要注册码。
> 如需真实授权，本机 `E:\AI及数智化\CadLicense\` 有一套现成的
> `CadLicense.Native`（C++，ECDSA P-256 + AES，纯 Win32，无第三方依赖）可以直接接入。
> 接入时建议把适配代码单独放一个 `.cpp`，**不要**在 `stdafx.h` 里 `#include cadlicense.h`
> ——`stdafx.h` 是预编译头，引入 `<string>/<vector>` 会与 MFC 的 `new`/`DEBUG_NEW` 机制打架。

---

## 三、顺带修掉的 R15 兼容问题

修好 `LicenseAll.h` 之后，编译会继续暴露两个 R14→R15 的 API 变更。这两个同样在
`LicenseAll.h` 里一处收口，**不改任何业务代码**。

### 1. `acdbCurDwg` 不再是导出函数

16 个文件直接用 `acdbCurDwg()`（如 `DrawUtil.h`），R15 起该符号只在
`migrtion.h` 里定义为宏：

```c
#define acdbCurDwg acdbHostApplicationServices()->workingDatabase
```

`LicenseAll.h` 里按同样方式补这个宏。**刻意没有 `#include "migrtion.h"`**，因为它会连带
`#include adsmigr.h`，把 `ads_printf` / `ads_getstring` / `ads_malloc` … 整族符号全部改名
（`ads_printf` → `acutPrintf` 等），属于大面积改动、且会与源码里已有的 `acedXxx` 写法重复。

```cpp
#if !defined(acdbCurDwg)
    #include <dbapserv.h>       // 声明 acdbHostApplicationServices()
    #define acdbCurDwg acdbHostApplicationServices()->workingDatabase
#endif
```

### 2. `ACRX_DXF_DEFINE_MEMBERS` 参数个数变了

工程代码用的是 **R14 时代的 5 参数**写法：

```cpp
ACRX_DXF_DEFINE_MEMBERS(CDwgParamObj, AcDbObject, 0, ContourDwgParam, Contour ARX Application);
```

R15 及以后的 `rxboiler.h` 改成了 **7 参数**（多了 `DWG_VERSION`、`MAINTENANCE_VERSION`）：

```cpp
ACRX_DXF_DEFINE_MEMBERS(类名, 父类名, DWG版本, 维护版本, PROXY_FLAGS, DXF名, APP名)
```

多出来的两个版本号只写进 `AcRxClass` 描述，对类注册与 DXF 名没有实质影响。
`LicenseAll.h` 里另起宏名 `ACRX_DXF_DEFINE_MEMBERS5` 提供 5 参数版本，
内部走 `newAcRxClass()` 的 3 参数重载，与 SDK 原版行为一致：

```cpp
#define ACRX_DXF_DEFINE_MEMBERS5(CLASS_NAME,PARENT_CLASS,PROXY_FLAGS,DXF_NAME,APP) \
ACRX_DEFINE_MEMBERS(CLASS_NAME); \
static AcRxObject * make##CLASS_NAME() { return new CLASS_NAME(); } \
void CLASS_NAME::rxInit() { \
    ACRX_STATIC_CHECK(CLASS_NAME); \
    CLASS_NAME::gpDesc = newAcRxClass(#CLASS_NAME, #PARENT_CLASS, \
        PROXY_FLAGS, &make##CLASS_NAME, #DXF_NAME, #APP); \
}
```

7 处调用点各改一个宏名即可（`ACRX_DXF_DEFINE_MEMBERS` → `ACRX_DXF_DEFINE_MEMBERS5`）。

---

## 四、链接期问题（LNK1104 / LNK1120）与解决

### 1. LNK1104: cannot open file "License.lib"

`License.lib` 是私有授权库，不可得。**解决：** 见第二节 —— 把 4 个类桩在
`LicenseAll.h` 里后，`.dsp` 链接行直接删掉 `License.lib`，链接期不再需要它。

### 2. LNK1120: unresolved externals（去掉 License.lib 后暴露出来的）

去掉 `License.lib` 后第一次链接暴露 **329 个未解析符号**。逐个定位后发现
**没有一个来自 `License.lib`**，全部是两类问题：

#### (a) 缺 ObjectARX R15 的库（多数）

`.dsp` 只写了 `acad.lib rxapi.lib acedapi.lib acge15.lib acgiapi.lib`，
而实际需要的 i386 实现分别在：

| 缺失库 | 提供的内容 |
|---|---|
| `acdb15.lib` | `AcDbObject` / `AcDbEntity` / `AcDbText` / `AcDbLine` … 全部数据库类 |
| `acrx15.lib` | `newAcRxClass` / `acrxBuildClassHierarchy` / `deleteAcRxClass` |
| `acutil15.lib` | `acutPrintf` / `acutPolar` / `acutAngle` / `acrx_abort` |

再叠加 `Libacge.lib` → `acge15.lib`、`Acgiapi.lib` → `acgiapi.lib`（R14 旧名 → R15 新名）
的改名。补上后 **329 → 16**。

#### (b) GridCtrl 的 10 个 `.cpp`（15 个符号）

链接时 `CGridCtrl` / `CGridCell` / `CGridCellCombo` 报了 15 个未解析符号。

**注意：这一项的 `.dsp` 本身没有问题** —— `GridControlSource` 组里
10 个 `.cpp`（`GridCell.cpp` / `GridCellBase.cpp` / `GridCtrl.cpp` /
`GridDropTarget.cpp` / `InPlaceEdit.cpp` / `TitleTip.cpp` /
`GridCellCheck.cpp` / `GridCellCombo.cpp` / `GridCellDateTime.cpp` /
`GridURLCell.cpp`）都已正确登记。

之所以在诊断过程中出现这批符号，是因为 `build.sh` 最初的源文件通配只覆盖了
`Source\*.cpp`，没有递归到 `GridControlSource\` 子目录。
修正通配后 **16 → 1**。

> 结论：**在 IDE 里打开 `.dsp` 编译不会遇到这 15 个符号**，
> 这是本次命令行脚本的疏漏，不算工程问题。

#### (c) `_hdllInstance` 未定义（最后 1 个）

`ContourArx.cpp:48` 只有声明：

```cpp
extern "C" HINSTANCE _hdllInstance;
```

但它被 `initApp()` / `unloadApp()` 传给 `InitModule()`（标准 MFC 扩展 DLL 初始化流程：
`AfxInitExtensionModule` + `new CDynLinkLibrary`）。**必须先有 HINSTANCE 才能初始化。**

全工程搜遍只有声明、没有定义；全 VC98 的 274 个库文件中也没有任何一个提供该符号
（已用二进制搜索确认）——说明原始定义随未发布的 `LicenseKeyGenerator` 一起丢了。

**解决（`ContourArx.cpp` 内 1 处定义 + 1 个 `DllMain`）：** 这是标准 MFC 扩展 DLL 模板的
要求，补回即可：

```cpp
extern "C" HINSTANCE _hdllInstance = NULL;

extern "C" BOOL __stdcall DllMain(HINSTANCE hInstance, unsigned long dwReason, void* lpReserved)
{
    if (dwReason == 1 /*DLL_PROCESS_ATTACH*/)
        _hdllInstance = hInstance;
    return TRUE;
}
```

> 注意两点踩坑：
> 1. 必须**显式写 `__stdcall`**，不要用 `WINAPI` 宏 —— 否则 VC6 会报
>    `C2447: missing function header (old-style formal list?)`。
> 2. 不能用 `__ImageBase` —— 那是 MSVC 7.0+ 才有的符号，VC6 报 `C2065`。
> 3. `DllMain` 是**唯一的** `_hdllInstance` 赋值点；**不要**改成
>    `GetModuleHandle(NULL)`，那拿到的是宿主 `AutoCAD.exe` 的句柄。

### 3. /entry:"DllEntryPoint@12" 必须去掉

`.dsp` 原链接行带 `/entry:"DllEntryPoint@12"`（R14 时代的 ARX 入口名）。
R15 的 ARX 是标准 MFC 扩展 DLL，入口就是 `DllMain` + 导出的 `acrxEntryPoint`。
保留 `/entry:"DllEntryPoint@12"` 会直接产生 `LNK2001: __hdllInstance` 与
`LNK1120: unresolved external symbol _DllEntryPoint@12`。

**对照证据**：本机的另一个真实可编译的 ARX 工程
`D:\SoftwareDevelop\四院科研成果\CASDVSS\TSYRSD2003\`，其 `.dsp` 的链接行
**没有** `/entry:` 参数，且库列表是完整的 R15 家族。

---

## 五、编译环境（本机已验证）

| 项 | 路径 |
|---|---|
| VC6 编译器 | `D:\SoftwareDevelop\Microsoft Visual Studio-zbh\VC98` |
| VC6 DLL / `RC.EXE` / `DUMPBIN.EXE`（必须加进 PATH） | `D:\SoftwareDevelop\Microsoft Visual Studio-zbh\Common\MSDev98\Bin` |
| ObjectARX R15 SDK（32 位，VC6 可用） | `D:\SoftwareDevelop\四院科研成果\CASDVSS\TSYRSD2003\{inc,lib}` |

`INCLUDE` 需要包含：

```
<VC98>\Include ; <VC98>\MFC\Include ; <VC98>\ATL\Include
; <ARX15>\inc
; <Source> ; <Source>\GridControlSource\GridCtrl_src ; <Source>\GridControlSource\NewCellTypes
```

`LIB` 需要包含：

```
<VC98>\Lib ; <VC98>\MFC\Lib ; <ARX15>\lib
```

### 构建踩坑记录

| 现象 | 原因 / 解决 |
|---|---|
| `cl.exe: error while loading shared libraries: ?: cannot open shared object file`（exit 127） | PATH 里缺 `Common\MSDev98\Bin`，只加 `VC98\Bin` 不够 |
| Git Bash 下调 `link` 无效 | MSYS 的 `/usr/bin/LINK.EXE`（GNU coreutils `link`）会遮蔽 MSVC 的 linker，**必须用绝对路径调用** |
| `LNK1136: invalid or corrupt file`（对 `ContourArx.def`） | 本环境下 VC6 linker 对**裸的** `x.def` 参数解析异常；改写成 **`/DEF:ContourArx.def`** 即可 |
| `RC : fatal error RC1106 : invalid option: -ologo` | `/nologo` 被 Bash 处理坏；RC 调用不加 `/nologo` |
| 中文路径/反斜杠被 Bash 改写（`E:\x` → `E:////x`） | 用 Python 脚本写文件（`chr(92)`），或 `MSYS_NO_PATHCONV=1`；`.exe` 调用前把路径转成 `/d/...` 形式 |
| INCLUDE/LIB 里写了 `/` 分隔的路径 → `C1083: Cannot open include file: 'afxwin.h'` | VC6 的 INCLUDE/LIB **必须用 Windows 反斜杠 + 分号**；而调用 `cl.exe` 本身要用 MSYS 的 `/d/...` 形式。两者要分开准备 |
| 编译器输出乱码 | VC6 输出是 GBK，用 `iconv -f GBK -t UTF-8` 转 |

### 源文件编码约定（重要，容易踩坑）

**结论：本工程所有参与编译的源文件一律用纯 ASCII；绝不要用「UTF-8 with BOM」。**

实测三种编码喂给 VC6（12.00.8168，1998 年版）的结果：

| 源文件编码 | VC6 编译 | 说明 |
|---|---|---|
| **UTF-8 with BOM** | **失败** | 报 `error C2018: unknown character '0xef'`（依次还有 `0xbb`/`0xbf`）。**VC6 不认识 BOM，会把它当代码字符。** |
| UTF-8 无 BOM | 通过 | 但中文只出现在注释里才安全；一旦进入字符串字面量，运行期会显示乱码 |
| GBK | 通过 | 中文 Windows 下正常，但 GitHub / VS Code 默认按 UTF-8 解析会显示乱码 |
| **纯 ASCII** | 通过 | **本工程采用这一种**，任何编辑器与平台都不会乱码 |

另外，**VC6 完全没有 `/source-charset` 之类的开关**（该选项是 VS2015+ 才有的），
它根本没有"源文件编码"这个概念，只是把字节原样吞进去 —— 所以编码问题只能靠
「统一写 ASCII」来从根上避免。

本工程 184 个源文件中 181 个是纯 ASCII，所以 `LicenseAll.h` 也按纯 ASCII 编写
（原先是 GBK，在 GitHub / VS Code / 预览窗口里显示为乱码，已改为英文注释）。
编码敏感的文件现状：

| 文件 | 编码 | 原因 |
|---|---|---|
| `LicenseAll.h` | 纯 ASCII | 参与编译；已在 GitHub 显示乱码，故改英文注释 |
| `ContourArx.dsp` / `.def` / `.dsw` | 纯 ASCII | VC6 直接解析这些工程文件 |
| `ContourArx.cpp` | GBK | 参与编译；沿用原始工程的 GBK 约定，中文 Windows 下正常 |
| `build.sh` | UTF-8 | 不参与编译；shell 脚本，GitHub 按 UTF-8 渲染 |
| `BUILD-移植说明.md` | UTF-8 | 不参与编译；Markdown 文档，GitHub 按 UTF-8 渲染 |

> 若后续要新增/修改**参与编译**的源文件，请一律写纯 ASCII 注释，
> 或至少确认 VC6 能编过（`sh build.sh` 会立刻暴露 `C2018`）。
> 任何时候都不要给源文件加 UTF-8 BOM。

### 编译验证结果

用与 `.dsp` 完全相同的开关（`/MD /W3 /Gi /GX /Od /D NDEBUG /D RADPACK /D ACRXAPP
/D STRICT /D WIN32 /D _WINDOWS /D _WINDLL /D _AFXDLL`，`/Yc` `/Yu"stdafx.h"`）：

```
共 69 个源文件, 失败 0 个
链接退出码: 0
未解析符号数: 0
```

### 产物校验

`Release/ContourArx.arx` —— 749,568 字节，合法 32 位 PE DLL
（`machine=0x014C`，`characteristics=0x210E` 含 `IMAGE_FILE_DLL`），
导出表：

```
ordinal hint RVA      name
      1    0 0007CA30 _SetacrxPtp
      2    1 00001816 acrxEntryPoint
      3    2 0007C940 acrxGetApiVersion
```

三个导出与 ObjectARX ARX 模块要求一致，可用 `APPLOAD` 载入 AutoCAD 2000/2002/2004 系列。

---

## 六、遗留说明

- `ProtectApp.cpp` 有未完成的非法语句 `return TRUE/FALSE;`，且 `#include "Junk.h"` 指向
  一个**不存在的头文件**。这是原作者遗留的死代码，**不参与构建**（`build.sh` 里显式跳过）。
  建议后续要么补全、要么从工程移除。
- `CProtection`（`Protection.cpp/-h`）是**独立于 `License.lib` 的另一套自研保护机制**，
  不依赖缺失的私有库，**未做任何改动**，保持原样工作。
- 编译产物（`*.arx` / `*.obj` / `*.pch` / `Release/` / `Debug/` / `_build_obj/` 等）
  已在 `.gitignore` 中排除。
