#include "platform/platform_current_geo_location.h"
#include "core/current_geo_location.h"

namespace Platform {

void ResolveCurrentExactLocation(Fn<void(Core::GeoLocation)> callback) {
	if (callback) {
		callback({});
	}
}

void ResolveLocationAddress(
		const Core::GeoLocation &,
		const QString &,
		Fn<void(Core::GeoAddress)> callback) {
	if (callback) {
		callback({});
	}
}

} // namespace Platform
