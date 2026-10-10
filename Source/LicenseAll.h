// LicenseAll.h
////////////////////////////////////////////////////////////////
// Replacement for the original (vendor-private, never published)
// LicenseAll.h.
//
// The original project depends on a private licensing library under
//   <project-root>\..\..\LicenseKeyGenerator\License
// which provides CLicenseManager / CApplcationProtector /
// CProtectionInterfaceBase / CProtectionDebugging plus License.lib.
// That directory was never published to the Git repository, so the
// #include <LicenseAll.h> in stdafx.h fails with
//   fatal error C1083: Cannot open include file: 'LicenseAll.h'
//
// This header supplies the missing symbols with the smallest possible
// change, so the project can be compiled:
//   - CProtectionInterfaceBase : empty base class;
//   - CLicenseManager          : only AppType::ContourARX and static helpers;
//   - CApplcationProtector      : only the FaultType enum and no-op methods;
//                                 Initialize() always returns Success;
//   - CProtectionDebugging      : no-op methods.
//
// NOTE: cadlicense.h is deliberately NOT included here. Pulling
//       <string>/<vector> into the stdafx.h precompiled header conflicts
//       with the MFC new/DEBUG_NEW machinery. When wiring up real
//       CadLicense licensing, put the adapter code in its own .cpp file
//       (see the note at the end of this file).
//
// NOTE: This file is intentionally pure ASCII (no Chinese, no UTF-8 BOM).
//       VC6 has no source-charset option and rejects a UTF-8 BOM with
//       "error C2018: unknown character '0xef'".
////////////////////////////////////////////////////////////////
#ifndef _LICENSEALL_H_
#define _LICENSEALL_H_

#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// ObjectARX R15 compatibility
// ---------------------------------------------------------------------------
// This project was written against an AutoCAD R14 era SDK and calls
// acdbCurDwg() directly all over the place. Since ObjectARX R15,
// acdbCurDwg is no longer an exported function; the SDK only defines it
// as a macro inside migrtion.h:
//     acdbHostApplicationServices()->workingDatabase
//
// migrtion.h is deliberately NOT included here: it pulls in adsmigr.h,
// which renames the entire ads_* family (ads_prompt / ads_getstring /
// ads_printf / ads_malloc ...). That would be a sweeping change.
// Defining just this one macro keeps the change minimal.
//
// Requirements:
//   * dbapserv.h must be reachable, to declare acdbHostApplicationServices()
//     (that function exists since R15);
//   * acutil15.lib must be linked (it provides AcDbHostApplicationServices).
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

	// Read the license key file. Returns a string allocated with new[]
	// (the caller must delete[] it). Returns 0 on failure.
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

	// Validate the license key. The signing private key is not available
	// here, so this is the minimal viable check: non-empty.
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
// ACRX_DXF_DEFINE_MEMBERS argument-count compatibility
// ---------------------------------------------------------------------------
// The project code uses the R14 era 5-argument form:
//     ACRX_DXF_DEFINE_MEMBERS(CLASS, PARENT, PROXY_FLAGS, DXF_NAME, APP_NAME)
// but rxboiler.h in R15 and later takes 7 arguments:
//     ACRX_DXF_DEFINE_MEMBERS(CLASS, PARENT, DWG_VERSION, MAINTENANCE_VERSION,
//                             PROXY_FLAGS, DXF_NAME, APP_NAME)
// The extra DWG_VERSION / MAINTENANCE_VERSION only go into the AcRxClass
// description and have no real effect on class registration or DXF names.
//
// Below, after the SDK headers, a 5-argument variant is provided under a
// distinct macro name to avoid clashing with the SDK's 7-argument version.
// Usage: rename ACRX_DXF_DEFINE_MEMBERS to ACRX_DXF_DEFINE_MEMBERS5 at the
// call sites. It is implemented with ACRX_DEFINE_MEMBERS plus the 3-argument
// overload of newAcRxClass, matching the stock SDK behaviour (it registers
// the pseudoConstructor and dxfName the same way).
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
