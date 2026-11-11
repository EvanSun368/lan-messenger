/* MinGW has no Windows SDK icu.h / icu.lib. Forward Intl to BasicIntl. */
#include "twidget/icu_intl.h"

namespace twidget {

IcuIntl::IcuIntl(std::string fallbackLocale)
: fallback_(std::move(fallbackLocale)) {
}

std::string IcuIntl::format(const Json &request) {
	return basic_.format(request);
}

} // namespace twidget
