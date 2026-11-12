#include "check.h"
#include "step_policy.h"

LAN_TEST(step_timer_uses_short_nonzero_interval_only_while_spinning) {
	LAN_CHECK_EQ(step_timer_interval_ms(false), kIdleStepTimerMs);
	LAN_CHECK_EQ(step_timer_interval_ms(true), kSpinStepTimerMs);
	LAN_CHECK(kSpinStepTimerMs > 0);
	LAN_CHECK(kSpinStepTimerMs < kIdleStepTimerMs);
	LAN_CHECK(kSpinDrainMs > 0);
	LAN_CHECK(kSpinDrainMs < kIdleStepTimerMs);
}

LAN_TEST(peer_hints_equal_only_when_id_name_and_ip_match) {
	PeerHint a;
	a.id = QStringLiteral("aa");
	a.name = QStringLiteral("Bob");
	a.ip = QStringLiteral("10.0.0.2");
	PeerHint b = a;
	LAN_CHECK(peer_hints_equal({a}, {b}));
	b.name = QStringLiteral("Bobby");
	LAN_CHECK(!peer_hints_equal({a}, {b}));
	b = a;
	b.ip = QStringLiteral("10.0.0.3");
	LAN_CHECK(!peer_hints_equal({a}, {b}));
	LAN_CHECK(!peer_hints_equal({a}, {}));
}

LAN_TEST(stable_peer_upsert_does_not_dirty_hints_or_dialogs) {
	Dialog item;
	item.id = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	item.name = QStringLiteral("Bob");
	item.online = true;
	item.ip = QStringLiteral("10.0.0.2");
	item.firstName = QStringLiteral("Bob");
	item.lastName = QStringLiteral("Lee");
	item.bio = QStringLiteral("hi");
	const auto change = apply_peer_upsert(
		item,
		QStringLiteral("Bob"),
		true,
		QStringLiteral("10.0.0.2"),
		QStringLiteral("Bob"),
		QStringLiteral("Lee"),
		QStringLiteral("hi"));
	LAN_CHECK(!change.created);
	LAN_CHECK(!change.hintChanged);
	LAN_CHECK(!change.dialogChanged);
	LAN_CHECK(!change.cameOnline);
	LAN_CHECK(!change.identityBecameMergeable);
	LAN_CHECK(!should_merge_offline_leftovers(change, true));
}

LAN_TEST(peer_upsert_flags_first_sight_name_ip_and_online_changes) {
	Dialog item;
	item.id = QStringLiteral("cccccccccccccccccccccccccccccccc");
	item.name = QStringLiteral("cccccccc");
	item.online = false;

	auto created = PeerUpsertChange{};
	created.created = true;
	LAN_CHECK(should_merge_offline_leftovers(created, true));
	LAN_CHECK(!should_merge_offline_leftovers(created, false));

	const auto online = apply_peer_upsert(
		item,
		QStringLiteral("cccccccc"),
		true,
		{},
		{},
		{},
		{});
	LAN_CHECK(online.cameOnline);
	LAN_CHECK(online.dialogChanged);
	LAN_CHECK(!online.hintChanged);
	LAN_CHECK(should_merge_offline_leftovers(online, true));

	const auto named = apply_peer_upsert(
		item,
		QStringLiteral("Carol"),
		true,
		{},
		{},
		{},
		{});
	LAN_CHECK(named.hintChanged);
	LAN_CHECK(named.dialogChanged);
	LAN_CHECK(named.identityBecameMergeable);
	LAN_CHECK(should_merge_offline_leftovers(named, true));

	const auto ip = apply_peer_upsert(
		item,
		QStringLiteral("Carol"),
		true,
		QStringLiteral("10.0.0.5"),
		{},
		{},
		{});
	LAN_CHECK(ip.hintChanged);
	LAN_CHECK(ip.identityBecameMergeable);
	LAN_CHECK(should_merge_offline_leftovers(ip, true));

	const auto again = apply_peer_upsert(
		item,
		QStringLiteral("Carol"),
		true,
		QStringLiteral("10.0.0.5"),
		{},
		{},
		{});
	LAN_CHECK(!again.hintChanged);
	LAN_CHECK(!again.dialogChanged);
	LAN_CHECK(!should_merge_offline_leftovers(again, true));
}

LAN_TEST(transfer_ui_publishes_progress_at_most_every_interval) {
	LAN_CHECK(should_publish_transfer_ui(false, 0, 1));
	LAN_CHECK(should_publish_transfer_ui(false, 50, 51));
	LAN_CHECK(should_publish_transfer_ui(true, 0, 1));
	LAN_CHECK(should_publish_transfer_ui(true, 1000, 1100));
	LAN_CHECK(!should_publish_transfer_ui(true, 1000, 1099));
	LAN_CHECK(!should_publish_transfer_ui(true, 1000, 1000));
}
