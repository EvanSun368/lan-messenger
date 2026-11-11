#include "platform/win/windows_toast_activator.h"

QString ToastActivation::String(LPCWSTR value) {
	return value ? QString::fromWCharArray(value) : QString();
}

rpl::producer<ToastActivation> ToastActivations() {
	return rpl::never<ToastActivation>();
}
