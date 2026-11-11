/* Pregenerated from windows_quiethours.idl for MinGW / midl-less builds. */
#ifndef __WINDOWS_QUIETHOURS_H_H__
#define __WINDOWS_QUIETHOURS_H_H__

#include <windows.h>
#include <unknwn.h>
#include <oleauto.h>

#ifndef MIDL_INTERFACE
#define MIDL_INTERFACE(x) struct __declspec(uuid(x)) __declspec(novtable)
#endif

#ifndef __IQuietMoment_INTERFACE_DEFINED__
#define __IQuietMoment_INTERFACE_DEFINED__
EXTERN_C const IID IID_IQuietMoment;
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("af86e2e0-b12d-4c6a-9c5a-d7aa65101e90")
IQuietMoment : public IUnknown
{
};
#endif
#endif

#ifndef __IQuietHoursProfile_INTERFACE_DEFINED__
#define __IQuietHoursProfile_INTERFACE_DEFINED__
EXTERN_C const IID IID_IQuietHoursProfile;
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("e813fe81-62b6-417d-b951-9d2e08486ac1")
IQuietHoursProfile : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE get_DisplayName(LPWSTR *displayName) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_ProfileId(LPWSTR *profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSetting(int setting, int *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE PutSetting(int setting, int value) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsCustomizable(BOOL *result) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAllowedContacts(UINT32 *count, LPWSTR *allowedContacts) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddAllowedContact(LPWSTR allowedContact) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveAllowedContact(LPWSTR allowedContact) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAllowedApps(UINT32 *count, LPWSTR **allowedApps) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddAllowedApp(LPWSTR allowedApp) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveAllowedApp(LPWSTR allowedApp) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Description(LPWSTR *description) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_CustomizeLinkText(LPWSTR *linkText) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_RestrictiveLevel(LPWSTR *restrictiveLevel) = 0;
};
#endif
#endif

#ifndef __IQuietHoursPinnedContactManager_INTERFACE_DEFINED__
#define __IQuietHoursPinnedContactManager_INTERFACE_DEFINED__
EXTERN_C const IID IID_IQuietHoursPinnedContactManager;
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("cd86a976-8ea9-404b-a197-42e73dbaa901")
IQuietHoursPinnedContactManager : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetPinnedContactList(UINT32 *count, LPWSTR *pinnedContacts) = 0;
};
#endif
#endif

#ifndef __IQuietMomentsManager_INTERFACE_DEFINED__
#define __IQuietMomentsManager_INTERFACE_DEFINED__
EXTERN_C const IID IID_IQuietMomentsManager;
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("b0217783-87b7-422c-b902-5c148c14f150")
IQuietMomentsManager : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetAllQuietMomentModes(UINT32 *count, UINT32 **quietMomentModes) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetQuietMoment(UINT32 quietMomentId, IQuietMoment **quietMoment) = 0;
	virtual HRESULT STDMETHODCALLTYPE TurnOffCurrentlyActiveQuietMoment() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetActiveQuietMoment(UINT32 *quietMomentId) = 0;
};
#endif
#endif

typedef struct _QH_PROFILE_DATA
{
	char do_not_use__incomplete;
} QH_PROFILE_DATA;

#ifndef __IQuietHoursSettings_INTERFACE_DEFINED__
#define __IQuietHoursSettings_INTERFACE_DEFINED__
EXTERN_C const IID IID_IQuietHoursSettings;
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("6bff4732-81ec-4ffb-ae67-b6c1bc29631f")
IQuietHoursSettings : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE get_UserSelectedProfile(LPWSTR *profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE put_UserSelectedProfile(LPWSTR profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProfile(LPWSTR profileId, IQuietHoursProfile **) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAllProfileData(UINT32 *count, QH_PROFILE_DATA *) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayNameForProfile(LPWSTR profileId, LPWSTR *displayName) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_QuietMomentsManager(IQuietMomentsManager **) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_OffProfileId(LPWSTR *profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_ActiveQuietMomentProfile(LPWSTR *profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE put_ActiveQuietMomentProfile(LPWSTR profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_ActiveProfile(LPWSTR *profileId) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_QuietHoursPinnedContactManager(IQuietHoursPinnedContactManager **) = 0;
	virtual HRESULT STDMETHODCALLTYPE put_QuietMomentsShowSummaryEnabled(BOOL *isEnabled) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAlwaysAllowedApps(UINT32 *count, LPWSTR **allowedApps) = 0;
	virtual HRESULT STDMETHODCALLTYPE StartProcessing() = 0;
	virtual HRESULT STDMETHODCALLTYPE StopProcessing() = 0;
};
#endif
#endif

EXTERN_C const CLSID CLSID_QuietHoursSettings;
#ifdef __cplusplus
class DECLSPEC_UUID("f53321fa-34f8-4b7f-b9a3-361877cb94cf") QuietHoursSettings;
#endif

#endif /* __WINDOWS_QUIETHOURS_H_H__ */
