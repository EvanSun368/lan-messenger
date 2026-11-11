#pragma once

// This fork is a serverless LAN messenger. Official Telegram login and
// production DCs must not be the default path of a stock build.
namespace Lan {

inline constexpr bool kServerless = true;
inline constexpr auto kProductName = "LanLink";
inline constexpr auto kIdentityFolder = "lan_identity";

} // namespace Lan
