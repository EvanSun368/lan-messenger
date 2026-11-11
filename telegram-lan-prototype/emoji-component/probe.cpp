#include <cstring>
#include <QCoreApplication>
#include <QDebug>
#include "emoji_suggestions.h"
int main(int argc,char**argv){QCoreApplication app(argc,argv); for(auto s: {QString("smile"),QString(":smile"),QString("heart"),QString("dog")}) {auto r=Ui::Emoji::GetSuggestions({reinterpret_cast<const unsigned short*>(s.utf16()),std::size_t(s.size())});qDebug()<<s<<r.size();if(r.empty())return 1;}return 0;}
