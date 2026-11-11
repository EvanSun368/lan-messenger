/* MinGW stub: no Microsoft WebView2 loader. Mini-apps / IV webviews stay unavailable. */
#include "webview/platform/win/webview_windows_edge_chromium.h"
#include "webview/platform/win/webview_windows_edge_html.h"

namespace Webview::EdgeChromium {

bool Supported() {
	return false;
}

std::unique_ptr<Interface> CreateInstance(Config) {
	return nullptr;
}

} // namespace Webview::EdgeChromium

namespace Webview::EdgeHtml {

bool Supported() {
	return false;
}

std::unique_ptr<Interface> CreateInstance(Config) {
	return nullptr;
}

} // namespace Webview::EdgeHtml
