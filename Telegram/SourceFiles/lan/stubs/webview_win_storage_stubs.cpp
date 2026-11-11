#include "webview/webview_interface.h"

namespace Webview {

bool DeferTemporaryStorageRemoval(const QString &) {
	return false;
}

void ClearStaleTemporaryStorage() {
}

} // namespace Webview
