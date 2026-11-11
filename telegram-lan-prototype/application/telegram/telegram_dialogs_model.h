/*
 * Adapted from Telegram Desktop dialogs/dialogs_list.cpp at
 * c4bd064bcc84a359179e191fa6dfd216acda504f.
 * Copyright (c) 2014-2026 The Telegram Desktop Authors.
 * GPL-3.0-or-later with upstream OpenSSL exception. See ../../LICENSE and LEGAL.
 * Adaptation: LAN IDs/QString rows replace Telegram Key/Entry/Row, standard
 * algorithms replace ranges/not_null; no Session/MTProto is required.
 */
#pragma once
#include <QString>
#include <vector>
#include <algorithm>
#include <QHash>
#include <memory>
#include <map>
#include <QSet>

namespace TelegramLan {
struct DialogRow {
 QString id,name,preview,timeLabel;
 quint64 sortKey=0;
 int unread=0,index=0,top=0,height=76;
 bool online=false;
};
class DialogsModel {
public:
 enum class SortMode { Date, Name };
 explicit DialogsModel(SortMode mode=SortMode::Date):mode(mode){}
 const std::vector<DialogRow*>& rows() const {return ordered;}
 int height() const {return ordered.empty()?0:ordered.back()->top+ordered.back()->height;}
 DialogRow* find(const QString &id) const {auto i=owned.find(id);return i==owned.end()?nullptr:i->second.get();}
 DialogRow* upsert(DialogRow value) {
  auto row=find(value.id);
  if(!row){auto p=std::make_unique<DialogRow>();row=p.get();owned.emplace(value.id,std::move(p));row->index=int(ordered.size());row->top=height();ordered.push_back(row);}
  int index=row->index,top=row->top;
  *row=std::move(value);row->index=index;row->top=top;row->height=std::max(32,row->height);
  // Height changes propagate to following rows (adapted updateHeight).
  for(auto i=ordered.begin()+row->index;i!=ordered.end();++i){(*i)->top=top;top+=(*i)->height;}
  if(mode==SortMode::Date)adjustByDate(row);else adjustByName(row);
  return row;
 }
 void setSortMode(SortMode value){mode=value;
  std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return mode==SortMode::Date?a->sortKey>b->sortKey:a->name.compare(b->name,Qt::CaseInsensitive)<0;});reindex();}
 bool remove(const QString &id){auto i=owned.find(id);if(i==owned.end())return false;
  auto row=i->second.get();ordered.erase(ordered.begin()+row->index);owned.erase(i);reindex();return true;}
 // Upstream rowAtY/findByY algorithm, replacing ranges with std.
 DialogRow* rowAtY(int y) const {
  auto i=std::lower_bound(ordered.begin(),ordered.end(),y,[](const DialogRow* row,int value){return row->top+row->height-1<value;});
  if(i==ordered.end())return nullptr;auto row=*i;
  return row->top<=y&&row->top+row->height>y?row:nullptr;
 }
private:
 SortMode mode;std::map<QString,std::unique_ptr<DialogRow>> owned;std::vector<DialogRow*> ordered;
 using Iterator=std::vector<DialogRow*>::iterator;
 // Upstream rotate also fixes index and vertical positions.
 void rotate(Iterator first,Iterator middle,Iterator last){
  if(first==last)return;auto top=(*first)->top;std::rotate(first,middle,last);
  auto count=last-first;auto index=first-ordered.begin();
  while(count--){auto row=*first++;row->index=int(index++);row->top=top;top+=row->height;}
 }
 void reindex(){int top=0;for(int i=0;i<int(ordered.size());++i){ordered[i]->index=i;ordered[i]->top=top;top+=ordered[i]->height;}}
 // Adapted directly from List::adjustByDate. Frozen lists/pinning are not in this slice.
 void adjustByDate(DialogRow* row){
  auto key=row->sortKey;auto i=ordered.begin()+row->index;
  if(i+1!=ordered.end()&&(*(i+1))->sortKey>key){
   auto before=std::lower_bound(i+2,ordered.end(),key,[](DialogRow* r,quint64 k){return r->sortKey>k;});rotate(i,i+1,before);
  }else if(i!=ordered.begin()&&(*(i-1))->sortKey<key){
   auto after=std::lower_bound(ordered.begin(),i-1,key,[](DialogRow* r,quint64 k){return r->sortKey>=k;});rotate(after,i,i+1);
  }
 }
 // Adapted directly from List::adjustByName, case-insensitive for LAN names.
 void adjustByName(DialogRow* row){
  const auto &key=row->name;auto i=ordered.begin()+row->index;
  auto before=std::find_if(i+1,ordered.end(),[&](DialogRow*r){return r->name.compare(key,Qt::CaseInsensitive)>=0;});
  if(before!=i+1){rotate(i,i+1,before);}else if(i!=ordered.begin()){
   auto from=std::make_reverse_iterator(i);auto after=std::find_if(from,ordered.rend(),[&](DialogRow*r){return r->name.compare(key,Qt::CaseInsensitive)<=0;}).base();
   if(after!=i)rotate(after,i,i+1);
  }
 }
};
// Actual upstream ui/unread_counter_format.cpp algorithm; q-literals adapted.
inline QString FormatUnreadCounter(int unreadCounter,bool hasMentionOrReaction,bool narrow=false){
 if(unreadCounter<=0)return QString();
 if(!narrow)return QString::number(unreadCounter);
 if(hasMentionOrReaction&&unreadCounter>999)return QStringLiteral("99+");
 if(unreadCounter>999999)return QStringLiteral("99999+");
 return QString::number(unreadCounter);
}
inline QString FormatUnreadCounterShort(int unreadCounter){
 return unreadCounter<1'000?QString::number(unreadCounter)
 :unreadCounter<1'000'000?QString::number(unreadCounter/1'000)+'K'
 :unreadCounter<1'000'000'000?QString::number(unreadCounter/1'000'000)+'M'
 :QString::number(unreadCounter/1'000'000'000)+'B';
}
}
