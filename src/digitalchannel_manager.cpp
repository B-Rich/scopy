#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <vector>
#include <string.h>

#include <iio.h>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QFile>
#include <QtQml/QJSEngine>
#include <QtQml/QQmlEngine>
#include <QDirIterator>
#include <QPushButton>
#include <QFileDialog>

///* pulseview and sigrok */
#include <boost/math/common_factor.hpp>
#include "pulseview/pv/mainwindow.hpp"
#include "pulseview/pv/devices/binarybuffer.hpp"
#include "pulseview/pv/devicemanager.hpp"
#include "pulseview/pv/view/view.hpp"
#include "pulseview/pv/toolbars/mainbar.hpp"
#include "libsigrokcxx/libsigrokcxx.hpp"
#include "libsigrokdecode/libsigrokdecode.h"

//#include "pattern_generator.hpp"

#include "digitalchannel_manager.hpp"

using namespace std;
using namespace adiscope;

namespace pv {
class MainWindow;
class DeviceManager;
class Session;

namespace view {
class View;
class TraceTreeItem;
}
namespace toolbars {
class MainBar;
}
namespace widgets {
class DeviceToolButton;
}
}

namespace sigrok {
class Context;
}

namespace adiscope {

ChannelManager::ChannelManager()
{    
}
ChannelManager::~ChannelManager()
{

}

std::vector<int> ChannelManager::get_selected_indexes()
{
    unsigned int i=0;
    std::vector<int> selection;
    for(auto ch : channel_group)
    {
       if(ch->is_selected()) selection.push_back(i);
       i++;
    }
    return selection;
}

uint16_t ChannelManager::get_enabled_mask()
{
    unsigned int i=0;
    uint16_t ret=0;
    for(auto ch : channel_group)
    {
       if(ch->is_enabled()) ret = ret | ch->get_mask();
       i++;
    }
    return ret;
}

uint16_t ChannelManager::get_selected_mask(){
    unsigned int i=0;
    uint16_t ret=0;
    for(auto ch : channel_group)
    {
       if(ch->is_selected()) ret = ret | ch->get_mask();
       i++;
    }
    return ret;
}

std::vector<int> ChannelManager::get_enabled_indexes()
{
    unsigned int i=0;
    std::vector<int> selection;
    for(auto ch : channel_group)
    {
       if(ch->is_enabled()) selection.push_back(i);
       i++;
    }
    return selection;
}

std::vector<ChannelGroup*>* ChannelManager::get_channel_groups()
{
    return &channel_group;
}

ChannelGroup* ChannelManager::get_channel_group(int index)
{
    return channel_group[index];
}

void ChannelManager::deselect_all()
{
    for(auto&& ch : channel_group)
    {
        ch->select(false);
    }
}


Channel::Channel(uint16_t id_, std::string label_)
{
    label = label_;
    id =id_;
    mask = 1<<id_;
}
 Channel::~Channel()
 {

 }

ChannelUI::ChannelUI(Channel* ch, QWidget *parent) : QWidget(parent)
{
    this->ch = ch;
}
ChannelUI::~ChannelUI()
{}

Channel* ChannelUI::get_channel()
{
    return ch;
}

uint16_t Channel::get_mask()
{
    return mask;
}

uint16_t Channel::get_id()
{
    return id;
}

std::string Channel::get_label()
{
    return label;
}

void Channel::set_label(std::string label)
{
    this->label = label;
}

std::vector<Channel*>* ChannelGroup::get_channels()
{
    return &channels;
}

Channel* ChannelGroup::get_channel(int index)
{
    return channels[index];
}

bool ChannelGroup::is_selected() const
{
    return selected;
}

void ChannelGroup::select(bool value)
{
    selected = value;
}

void ChannelGroup::group(bool value)
{
    this->grouped = value;
}
void ChannelGroup::enable(bool value)
{
    this->enabled  = value;
}

bool ChannelGroup::is_grouped() const
{
    return grouped;
}

bool ChannelGroup::is_enabled() const
{
    return enabled;
}

ChannelGroup::ChannelGroup(Channel* ch)
{
    channels.push_back(ch);
    label = ch->get_label();
    group(false);
    select(false);
    enable(true);
}

ChannelGroup::ChannelGroup()
{
	group(false);
	select(false);
	enable(true);
}

ChannelGroup::~ChannelGroup()
{
    //qDebug()<<"ChannelGroup destroyed";
}

void ChannelGroup::set_label(std::string label)
{
    this->label = label;
}

std::string ChannelGroup::get_label()
{
    return label;
}

uint16_t ChannelGroup::get_mask()
{
    uint16_t mask = 0;
    for(auto i=0;i<channels.size();i++)
    {
        mask = mask | channels[i]->get_mask();
    }
    return mask;
}

void ChannelGroup::add_channel(Channel *channel)
{
    channels.push_back(channel);
}

void ChannelGroup::remove_channel(int chIndex)
{
	channels.erase(channels.begin() + chIndex);
}

size_t ChannelGroup::get_channel_count()
{
    return channels.size();
}

std::vector<uint16_t> ChannelGroup::get_ids()
{
    std::vector<uint16_t> ret;
    for(auto ch: (*get_channels()))
        ret.push_back(ch->get_id());
    return ret;
}

ChannelGroupUI::ChannelGroupUI(ChannelGroup* chg, QWidget *parent) : QWidget(parent)
{
    this->chg = chg;
}
ChannelGroupUI::~ChannelGroupUI()
{

}

ChannelGroup* ChannelGroupUI::get_group()
{
    return chg;
}

void ChannelGroupUI::select(bool selected)
{
    chg->select(selected);
}
void ChannelGroupUI::enable(bool enabled)
{
    chg->enable(enabled);
}

}