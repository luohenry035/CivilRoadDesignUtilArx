// LicenseAll.h
////////////////////////////////////////////////////////////////
// 替代原始（厂商私有、未随源码发布）的 LicenseAll.h。
//
// 原工程依赖 E:\AI及数智化\LicenseKeyGenerator\License 下的私有授权库
// （提供 CLicenseManager / CApplcationProtector / CProtectionInterfaceBase /
//   CProtectionDebugging 以及 License.lib），该目录未随 Git 仓库发布，
// 因此 stdafx.h 中的 #include <LicenseAll.h> 会报
//   fatal error C1083: Cannot open include file: 'LicenseAll.h'
//
// 本头文件只用最小改动补齐上述符号，使工程可编译：
//   - CProtectionInterfaceBase : 空基类；
//   - CLicenseManager           : 仅提供 AppType::ContourARX 与静态工具函数；
//   - CApplcationProtector      : 仅提供 FaultType 枚举与空实现，恒返回 Success；
//   - CProtectionDebugging      : 空实现。
//
// 注意：这里刻意不 include cadlicense.h，避免 stdafx.h 预编译头引入 <string>/<vector>
//       与 MFC 的 new/DEBUG_NEW 机制冲突。真正接入 CadLicense 授权时，
//       建议把适配代码单独放到一个 .cpp 里（见文件末尾说明）。
////////////////////////////////////////////////////////////////
#ifndef _LICENSEALL_H_
#define _LICENSEALL_H_

#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// ObjectARX R15 compatibility
// ---------------------------------------------------------------------------
// 本工程按 AutoCAD R14 时代的 SDK 编写，代码里到处直接用 acdbCurDwg()。
// 从 ObjectARX R15 起 acdbCurDwg 不再是导出函数，SDK 只在 migrtion.h 里
// 把它定义成宏 acdbHostApplicationServices()->workingDatabase。
//
// 这里刻意不 include "migrtion.h"：它会连带 include adsmigr.h，把
// ads_prompt / ads_getstring / ads_printf / ads_malloc … 整族符号全部改名，
// 属于大面积改动。只补这一个宏，改动面最小。
//
// 使用前提：
//   * 外部 dbapserv.h 保证 acdbHostApplicationServices() 已声明（该函数自 R15 起就在）；
//   * 链接 acutil15.lib（AcDbHostApplicationServices 所在库）。
#if !defined(acdbCurDwg)
	#include <dbapserv.h>
	#define acdbCurDwg acdbHostApplicationServices()->workingDatabase
#endif

//
// CProtectionInterfaceBase
/////////////////////////////////////////////////////////////////////////////
class CProtectionInterfaceBase
{
public:
	CProtectionInterfaceBase() {}
	virtual ~CProtectionInterfaceBase() {}
};

//
// CLicenseManager
/////////////////////////////////////////////////////////////////////////////
class CLicenseManager
{
public:
	enum AppType
	{
		ContourARX = 0
	};

	CLicenseManager(AppType, int = 0, int = 0) {}

	// 读取注册码文件。返回 malloc 出来的字符串（调用方 delete[]），失败返回 0。
	static bool ReadLiceseInfo(const char* pszFileName, char** ppszInfo)
	{
		if(ppszInfo != 0)
			*ppszInfo = 0;
		if(pszFileName == 0 || ppszInfo == 0)
			return false;

		FILE* pFile = fopen(pszFileName, "rb");
		if(pFile == 0)
			return false;

		fseek(pFile, 0, SEEK_END);
		long lSize = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		if(lSize <= 0)
		{
			fclose(pFile);
			return false;
		}

		char* pszBuf = new char[lSize + 1];
		size_t nRead = fread(pszBuf, 1, (size_t)lSize, pFile);
		pszBuf[nRead] = '\0';
		fclose(pFile);

		*ppszInfo = pszBuf;
		return true;
	}

	// 校验注册码。此处无签名私钥，仅做"非空"这一最小可行判定。
	bool CheckValidity(const char* pszInfo) const
	{
		return (pszInfo != 0 && pszInfo[0] != '\0');
	}
};

//
// CApplcationProtector
/////////////////////////////////////////////////////////////////////////////
class CApplcationProtector
{
public:
	enum FaultType
	{
		Success = 0,
		InvalidRegistry,
		InvalidLicense,
		InvalidLogFile,
		UserCancelledActivation
	};

	CApplcationProtector(CLicenseManager::AppType, CProtectionInterfaceBase* = 0) {}

	FaultType Initialize(char** ppszLicenseInfoForUser)
	{
		if(ppszLicenseInfoForUser != 0)
			*ppszLicenseInfoForUser = 0;
		return Success;
	}

	bool ResetLicenseRelatedData(CLicenseManager::AppType) { return true; }

	static const char* GetDebugLogFileName(CLicenseManager::AppType) { return "ContourARX.log"; }
};

//
// CProtectionDebugging
/////////////////////////////////////////////////////////////////////////////
class CProtectionDebugging
{
public:
	static bool StartDebugLog(const char*, bool = false) { return true; }
	static void EndDebugLog() {}
};

// ---------------------------------------------------------------------------
// ACRX_DXF_DEFINE_MEMBERS 参数个数兼容
// ---------------------------------------------------------------------------
// 本工程代码用的是 R14 时代的 5 参数写法：
//     ACRX_DXF_DEFINE_MEMBERS(类名, 父类名, PROXY_FLAGS, DXF名, APP名)
// 而 R15 及以后的 rxboiler.h 改成了 7 参数：
//     ACRX_DXF_DEFINE_MEMBERS(类名, 父类名, DWG版本, 维护版本, PROXY_FLAGS, DXF名, APP名)
// 多出来的 DWG_VERSION / MAINTENANCE_VERSION 只是写进 AcRxClass 描述里，
// 对类的注册与 DXF 名没有实质影响。
//
// 这里在包含 SDK 头之后补一个 5 参数版本（另起一个宏名，避免与 SDK 的 7 参数
// 版本冲突）。用法：把源码里的 ACRX_DXF_DEFINE_MEMBERS 改成 ACRX_DXF_DEFINE_MEMBERS5。
// 实现用 ACRX_DEFINE_MEMBERS + newAcRxClass 的 3 参数重载，
// 与 SDK 原版行为一致（同样注册 pseudoConstructor 与 dxfName）。
#include <rxboiler.h>

#define ACRX_DXF_DEFINE_MEMBERS5(CLASS_NAME,PARENT_CLASS,PROXY_FLAGS,DXF_NAME,APP) \
ACRX_DEFINE_MEMBERS(CLASS_NAME); \
static AcRxObject * make##CLASS_NAME() { return new CLASS_NAME(); } \
void CLASS_NAME::rxInit() { \
    ACRX_STATIC_CHECK(CLASS_NAME); \
    CLASS_NAME::gpDesc = newAcRxClass(#CLASS_NAME, #PARENT_CLASS, \
        PROXY_FLAGS, &make##CLASS_NAME, #DXF_NAME, #APP); \
}

#endif //_LICENSEALL_H_
