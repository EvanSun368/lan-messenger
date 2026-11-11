/* Pregenerated from windows_toastactivator.idl for MinGW / midl-less builds. */
#ifndef __WINDOWS_TOASTACTIVATOR_H_H__
#define __WINDOWS_TOASTACTIVATOR_H_H__

#include <windows.h>
#include <unknwn.h>

#ifndef MIDL_INTERFACE
#define MIDL_INTERFACE(x) struct __declspec(uuid(x)) __declspec(novtable)
#endif

typedef struct _NOTIFICATION_USER_INPUT_DATA
{
	LPCWSTR Key;
	LPCWSTR Value;
} NOTIFICATION_USER_INPUT_DATA;

#ifndef __INotificationActivationCallback_INTERFACE_DEFINED__
#define __INotificationActivationCallback_INTERFACE_DEFINED__
EXTERN_C const IID IID_INotificationActivationCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("53E31837-6600-4A81-9395-75CFFE746F94")
INotificationActivationCallback : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Activate(
		/* [in, string] */ LPCWSTR appUserModelId,
		/* [in, string] */ LPCWSTR arguments,
		/* [in, unique] */ const NOTIFICATION_USER_INPUT_DATA *data,
		/* [in] */ ULONG count) = 0;
};
#else
typedef struct INotificationActivationCallbackVtbl
{
	BEGIN_INTERFACE
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(INotificationActivationCallback *This, REFIID riid, void **ppvObject);
	ULONG (STDMETHODCALLTYPE *AddRef)(INotificationActivationCallback *This);
	ULONG (STDMETHODCALLTYPE *Release)(INotificationActivationCallback *This);
	HRESULT (STDMETHODCALLTYPE *Activate)(INotificationActivationCallback *This, LPCWSTR appUserModelId, LPCWSTR arguments, const NOTIFICATION_USER_INPUT_DATA *data, ULONG count);
	END_INTERFACE
} INotificationActivationCallbackVtbl;
interface INotificationActivationCallback
{
	CONST_VTBL struct INotificationActivationCallbackVtbl *lpVtbl;
};
#endif
#endif /* __INotificationActivationCallback_INTERFACE_DEFINED__ */

#endif /* __WINDOWS_TOASTACTIVATOR_H_H__ */
