#include "emojipicker.h"
#include <QtWidgets>
#include <cstring>
#include "emoji_suggestions.h"
QList<QPair<QString,QStringList>> EmojiPicker::categories() {
    return {{"笑脸",QString("😀 😃 😄 😁 😆 😅 😂 🤣 😊 🙂 🙃 😉 😇 🥰 😍 🤩 😘 😗 😚 😋 😛 😜 🤪 😝 🤑 🤗 🤭 🤫 🤔 🤐 🤨 😐 😑 😶 😏 😒 🙄 😬 🤥 😌 😔 😪 🤤 😴 😷 🤒 🤕 🤢 🤮 🤧 🥵 🥶 🥴 😵 🤯 🤠 🥳 😎 🤓 🧐 😕 😟 🙁 ☹️ 😮 😯 😲 😳 🥺 😦 😧 😨 😰 😥 😢 😭 😱 😖 😣 😞 😓 😩 😫 😤 😡 😠 🤬 😈 👿 💀 👻 🤖").split(' ')},
    {"手势",QString("👋 🤚 🖐️ ✋ 🖖 👌 🤏 ✌️ 🤞 🤟 🤘 🤙 👈 👉 👆 👇 ☝️ 👍 👎 ✊ 👊 🤛 🤜 👏 🙌 👐 🤲 🤝 🙏 💪 ✍️ 💅 🤳 👀 👂 👃 👄").split(' ')},
    {"心情",QString("❤️ 🧡 💛 💚 💙 💜 🖤 🤍 🤎 💔 ❣️ 💕 💞 💓 💗 💖 💘 💝 💟 💌 💋 💯 💢 💥 💫 💦 💨 💬 💤 ✨ ⭐ 🌟 🔥 🎉 🎊 🎈 🎁 🏆 🥇").split(' ')},
    {"日常",QString("☀️ 🌤️ 🌧️ ⛈️ ❄️ 🌈 🌙 🌍 🌱 🌸 🌹 🌻 🌲 🐶 🐱 🐭 🐰 🦊 🐼 🐸 🐯 🦁 🐷 🐵 🐣 🦋 🐝 🐢 🐟 🍎 🍊 🍋 🍉 🍇 🍓 🍒 🍑 🍌 🍍 🥑 🍔 🍟 🍕 🍜 🍚 🍰 🍪 🍫 ☕ 🍵 🥤 🍺 🥂 ⚽ 🏀 🎮 🎵 🎤 🎧 📷 💻 📱 🖨️ 📁 📌 📝 📚 🚗 🚲 ✈️ 🚀 🏠").split(' ')},
    {"符号",QString("✅ ❎ ❌ ⭕ ❓ ❗ ‼️ ⁉️ ⚠️ 🚫 ⛔ 🔒 🔓 🔑 🔔 🔕 ⏰ ⌛ ⏳ 🔍 🔎 💡 🔧 ⚙️ 🔨 📢 📣 📎 🔗 ➕ ➖ ✖️ ➗ ⬆️ ⬇️ ⬅️ ➡️ ↩️ ↪️ ▶️ ⏸️ ⏹️ 🔄 🆗 🆕 🆒 🆘").split(' ')}};
}
EmojiPicker::EmojiPicker(QWidget *parent):QDialog(parent) {
    setWindowTitle("表情"); setObjectName("emojiPicker"); resize(480,330);
    auto *layout=new QVBoxLayout(this);
    m_search=new QLineEdit(this); m_search->setObjectName("emojiSearch");
    m_search->setPlaceholderText("搜索更多表情（英文，例如 smile、heart）");
    layout->addWidget(m_search);
    connect(m_search,&QLineEdit::textChanged,this,[this]{populate();});
    m_tabs=new QTabBar; m_tabs->setObjectName("emojiCategories"); m_tabs->addTab("最近");
    for(auto c:categories()) m_tabs->addTab(c.first);
    layout->addWidget(m_tabs);
    m_list=new QListWidget; m_list->setObjectName("emojiList");
    m_list->setViewMode(QListView::IconMode); m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static); m_list->setGridSize(QSize(48,44));
    m_list->setFont(QFont("Segoe UI Emoji",20)); layout->addWidget(m_list);
    auto *hint=new QLabel("Unicode 表情 · 显示效果取决于系统字体"); layout->addWidget(hint);
    connect(m_tabs,&QTabBar::currentChanged,this,[this]{populate();});
    connect(m_list,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){
        auto e=item->data(Qt::UserRole).toString();
        QSettings settings(QSettings::defaultFormat(),QSettings::UserScope,"LanLink","LanLinkTelegramLab"); auto recent=settings.value("recentEmojis").toStringList();
        recent.removeAll(e); recent.prepend(e); while(recent.size()>32)recent.removeLast(); settings.setValue("recentEmojis",recent);
        emit selected(e); accept();
    });
    m_tabs->setCurrentIndex(1); populate();
}
void EmojiPicker::populate() {
    m_list->clear();
    const auto query=m_search->text().trimmed();
    m_tabs->setEnabled(query.isEmpty());
    if(!query.isEmpty()) {
        const auto suggestions=Ui::Emoji::GetSuggestions({reinterpret_cast<const unsigned short*>(query.utf16()),std::size_t(query.size())});
        QSet<QString> seen;
        for(const auto &s:suggestions) {
            const auto e=QString::fromUtf16(reinterpret_cast<const char16_t*>(s.emoji().data()),qsizetype(s.emoji().size()));
            if(seen.contains(e)) continue;
            seen.insert(e);
            auto *item=new QListWidgetItem(e,m_list); item->setData(Qt::UserRole,e);
            item->setToolTip(QString::fromUtf16(reinterpret_cast<const char16_t*>(s.label().data()),qsizetype(s.label().size())));
            item->setTextAlignment(Qt::AlignCenter);
        }
        return;
    }
    auto list=m_tabs->currentIndex()==0?QSettings(QSettings::defaultFormat(),QSettings::UserScope,"LanLink","LanLinkTelegramLab").value("recentEmojis").toStringList():categories().at(m_tabs->currentIndex()-1).second;
    for(auto e:list){ auto *item=new QListWidgetItem(e,m_list); item->setData(Qt::UserRole,e); item->setTextAlignment(Qt::AlignCenter); }
}
