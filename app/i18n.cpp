#include "i18n.h"

#include <QHash>
#include <QLocale>

namespace i18n {
namespace {

Language g_choice = Language::System;
Language g_resolved = Language::English;

struct Pack {
	const char *en;
	const char *zh_hans;
	const char *zh_hant;
};

const QHash<QString, Pack> kPack = {
	{"write_a_message", {"Write a message...", "输入消息...", "輸入訊息..."}},
	{"message_deleted", {"Message deleted", "消息已删除", "訊息已刪除"}},
	{"delete_for_everyone", {"Recall", "撤回", "撤回"}},
	{"recall", {"Recall", "撤回", "撤回"}},
	{"cancel", {"Cancel", "取消", "取消"}},
	{"save", {"Save", "保存", "儲存"}},
	{"ok", {"OK", "确定", "確定"}},
	{"edit_profile", {"Edit Profile", "编辑资料", "編輯個人資料"}},
	{"first_name", {"First name", "名字", "名字"}},
	{"last_name", {"Last name", "姓氏", "姓氏"}},
	{"bio", {"Bio", "个人简介", "個人簡介"}},
	{"set_photo", {"Set Photo", "设置照片", "設定照片"}},
	{"choose_photo", {"Choose photo", "选择照片", "選擇照片"}},
	{"new_group", {"New Group", "新建群组", "新增群組"}},
	{"night_mode", {"Night Mode", "夜间模式", "夜間模式"}},
	{"day_mode", {"Day Mode", "日间模式", "日間模式"}},
	{"settings", {"Settings", "设置", "設定"}},
	{"downloads", {"Downloads", "下载", "下載"}},
	{"about", {"About", "关于", "關於"}},
	{"about_title", {"About LanLink", "关于 LanLink", "關於 LanLink"}},
	{"search", {"Search", "搜索", "搜尋"}},
	{"file", {"File", "文件", "檔案"}},
	{"photo", {"Photo", "图片", "圖片"}},
	{"photo_or_video", {"Photo or video", "图片和视频", "圖片和影片"}},
	{"folder", {"Folder", "文件夹", "資料夾"}},
	{"copy", {"Copy", "复制", "複製"}},
	{"copy_text", {"Copy Text", "复制文本", "複製文字"}},
	{"open", {"Open", "打开", "開啟"}},
	{"send_file", {"Send file", "发送文件", "傳送檔案"}},
	{"send_folder", {"Send folder", "发送文件夹", "傳送資料夾"}},
	{"send_photo", {"Send photo", "发送图片", "傳送圖片"}},
	{"select_chat", {"Select a chat to start messaging", "选择一个聊天开始消息传送", "選擇一個聊天開始傳送訊息"}},
	{"on_lan", {"on LAN", "在局域网中", "在區域網路中"}},
	{"offline", {"offline", "不在线", "離線"}},
	{"cannot_send_offline", {"Can't send: the device is offline.", "无法发送：对方不在线。", "無法傳送：對方離線。"}},
	{"last_seen_recently", {"last seen recently", "最近上线过", "最近上線過"}},
	{"members", {"members", "位成员", "位成員"}},
	{"today", {"Today", "今天", "今天"}},
	{"yesterday", {"Yesterday", "昨天", "昨天"}},
	{"failed", {"Failed", "失败", "失敗"}},
	{"sticker", {"Sticker", "贴纸", "貼圖"}},
	{"language", {"Language", "语言", "語言"}},
	{"language_system", {"System default", "跟随系统", "跟隨系統"}},
	{"encrypt_files", {"Encrypt file transfers", "加密文件传输", "加密檔案傳輸"}},
	{"encrypt_files_about",
		{"Off by default on a trusted LAN. Text stays end-to-end encrypted. When this is off, file bytes travel in the clear so transfers are faster. Turn it on if you want file payloads sealed with tde2e.",
		 "默认关闭，适合受信任的局域网。文字始终端到端加密。关闭后，文件内容以明文传输以加快速度。如需用 tde2e 封装文件载荷，请打开此项。",
		 "預設關閉，適合受信任的區域網路。文字始終端對端加密。關閉後，檔案內容以明文傳輸以加快速度。如需以 tde2e 封裝檔案負載，請開啟此項。"}},
	{"profile_hint",
		{"Your name and bio are announced to peers on the LAN. A photo is stored on this computer and shown to you locally.",
		 "你的姓名和简介会在局域网中向对方公布。照片保存在本机，仅在本机显示。",
		 "你的姓名和簡介會在區域網路中向對方公布。照片保存在本機，僅在本機顯示。"}},
	{"about_body",
		{"A slim LAN messenger with a Telegram Desktop look. It does not log into Telegram and does not use Telegram's servers.\n\nPeers are discovered on the local network. Text is always sealed with official tde2e encryption. File-transfer encryption is optional and off by default. Encryption protects the payload, not membership of the LAN.\n\nAnimated stickers are not included in this slim build.",
		 "这是一款界面接近 Telegram Desktop 的轻量局域网即时通讯工具。它不会登录 Telegram，也不会连接 Telegram 的服务器。\n\n会在本地网络中发现对端。文字始终使用官方 tde2e 加密。文件传输加密为可选项，默认关闭。加密保护的是内容，并不能隐藏谁加入了局域网。\n\n此精简版不包含动态表情包。",
		 "這是一款介面接近 Telegram Desktop 的輕量區域網路即時通訊工具。它不會登入 Telegram，也不會連線到 Telegram 的伺服器。\n\n會在本機網路中發現對端。文字始終使用官方 tde2e 加密。檔案傳輸加密為可選項，預設關閉。加密保護的是內容，並不能隱藏誰加入了區域網路。\n\n此精簡版不包含動態貼圖。"}},
	{"lanlink_local", {"LanLink · local network", "LanLink · 本地网络", "LanLink · 本機網路"}},
	{"ip_label", {"IP", "IP 地址", "IP 位址"}},
	{"images_filter", {"Images (*.png *.jpg *.jpeg *.webp *.bmp)", "图片 (*.png *.jpg *.jpeg *.webp *.bmp)", "圖片 (*.png *.jpg *.jpeg *.webp *.bmp)"}},
	{"dir", {"DIR", "目录", "目錄"}},
	{"create", {"Create", "创建", "建立"}},
	{"add_members", {"Add Members", "添加成员", "新增成員"}},
	{"group_name", {"Group name", "群组名称", "群組名稱"}},
	{"no_messages_yet", {"No messages yet", "还没有消息", "還沒有訊息"}},
	{"group_created", {"Group created", "群组已创建", "群組已建立"}},
	{"cancelled", {"Cancelled", "已取消", "已取消"}},
	{"verified", {"Verified", "已校验", "已校驗"}},
	{"receive", {"Receive", "接收", "接收"}},
	{"decline", {"Decline", "拒绝", "拒絕"}},
	{"choose_save_location", {"Choose save location", "选择保存位置", "選擇儲存位置"}},
	{"save_folder", {"Default save folder", "默认保存位置", "預設儲存位置"}},
	{"save_folder_about",
		{"Incoming files are saved here when you receive them without picking a path.",
		 "接收文件时若未另外选择位置，将保存到此文件夹。",
		 "接收檔案時若未另外選擇位置，將儲存到此資料夾。"}},
	{"rejected", {"Declined to receive", "拒绝接收", "拒絕接收"}},
	{"waiting_to_accept", {"Waiting to accept", "等待接收", "等待接收"}},
	{"recall_file_in_use", {"Message deleted, but the file could not be removed because it is in use.", "消息已删除，但文件正在使用，未能从磁盘删除。", "訊息已刪除，但檔案正在使用，未能從磁碟刪除。"}},
	{"recall_file_failed", {"Message deleted, but the file could not be removed.", "消息已删除，但未能从磁盘删除文件。", "訊息已刪除，但未能從磁碟刪除檔案。"}},
	{"checksum_mismatch", {"Checksum mismatch", "校验失败", "校驗失敗"}},
	{"show_more", {"Show more", "显示更多", "顯示更多"}},
	{"show_less", {"Show less", "收起", "收起"}},
	{"incoming_file", {"Incoming file", "发来的文件", "傳來的檔案"}},
	{"version", {"Version %1", "版本 %1", "版本 %1"}},
};


Language detect() {
	const auto locale = QLocale::system();
	if (locale.language() != QLocale::Chinese) {
		return Language::English;
	}
	if (locale.script() == QLocale::TraditionalChineseScript
		|| locale.territory() == QLocale::Taiwan
		|| locale.territory() == QLocale::HongKong
		|| locale.territory() == QLocale::Macau) {
		return Language::ChineseTraditional;
	}
	return Language::ChineseSimplified;
}

} // namespace

void initFromLocale() {
	g_choice = Language::System;
	g_resolved = detect();
}

LanguageNotifier &notifier() {
	static LanguageNotifier instance;
	return instance;
}

void setLanguage(Language language) {
	g_choice = language;
	const auto previous = g_resolved;
	g_resolved = language == Language::System ? detect() : language;
	Q_UNUSED(previous);
	emit notifier().changed();
}

Language language() {
	return g_choice;
}

Language resolved() {
	return g_resolved;
}

bool isChinese() {
	return g_resolved == Language::ChineseSimplified
		|| g_resolved == Language::ChineseTraditional;
}

QString tr(const char *key) {
	const auto pack = kPack.value(QString::fromUtf8(key));
	if (pack.en == nullptr) {
		return QString::fromUtf8(key);
	}
	if (g_resolved == Language::ChineseSimplified) {
		return QString::fromUtf8(pack.zh_hans);
	}
	if (g_resolved == Language::ChineseTraditional) {
		return QString::fromUtf8(pack.zh_hant);
	}
	return QString::fromUtf8(pack.en);
}


QString localize(const QString &keyOrValue) {
	if (keyOrValue.isEmpty()) {
		return {};
	}
	if (kPack.contains(keyOrValue)) {
		return tr(keyOrValue.toUtf8().constData());
	}
	for (auto it = kPack.constBegin(); it != kPack.constEnd(); ++it) {
		const auto &pack = it.value();
		if (pack.en == nullptr) {
			continue;
		}
		if (keyOrValue == QString::fromUtf8(pack.en)
			|| keyOrValue == QString::fromUtf8(pack.zh_hans)
			|| keyOrValue == QString::fromUtf8(pack.zh_hant)) {
			return tr(it.key().toUtf8().constData());
		}
	}
	const auto lower = keyOrValue.toLower();
	if (lower == QStringLiteral("declined")
		|| lower == QStringLiteral("rejected")
		|| lower == QStringLiteral("declined to receive")) {
		return tr("rejected");
	}
	if (lower == QStringLiteral("canceled")
		|| lower == QStringLiteral("cancelled")
		|| lower == QStringLiteral("peer aborted the transfer")
		|| lower == QStringLiteral("peer aborted the folder")) {
		return tr("cancelled");
	}
	return keyOrValue;
}

QString languageName(Language language) {
	switch (language) {
	case Language::System:
		return tr("language_system");
	case Language::English:
		return QStringLiteral("English");
	case Language::ChineseSimplified:
		return QStringLiteral("简体中文");
	case Language::ChineseTraditional:
		return QStringLiteral("繁體中文");
	}
	return {};
}

} // namespace i18n
