/*
 * Copyright 2016 Analog Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file LICENSE.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef DIGITAL_IO_H
#define DIGITAL_IO_H

#include <QWidget>
#include <QPushButton>
#include <QVector>
#include <vector>
#include <string>
#include <QList>
#include <QPair>
#include <QTimer>
#include "filter.hpp"
#include "digitalchannel_manager.hpp"

#include "apiObject.hpp"
#include "dynamicWidget.hpp"

// Generated UI
#include "ui_digitalio.h"
#include "ui_digitalIoElement.h"
#include "ui_digitalIoChannel.h"


extern "C" {
	struct iio_context;
	struct iio_device;
	struct iio_channel;
	struct iio_buffer;
}

namespace Ui {
class DigitalIO;
class dioElement;
class dioGroup;
}

namespace adiscope {
class DigitalIO;
class DigitalIO_API;
/*
class DigitalIoChannel
{
	Q_OBJECT
	int channel_id;
	int io;
public:
	DigitalIoChannel(int channel_id,int io);
	~DigitalIoChannel();
};*/

class DigitalIoGroup : public QWidget
{
	Q_OBJECT
	int nr_of_channels;
	int ch_mask;
	int channels;
//	QList channels;
	int io_mask;
	int mode;
	DigitalIO *dio;
public:
	DigitalIoGroup(QString label, int ch_mask, int io_mask, DigitalIO *dio,
	               QWidget *parent=0);
	~DigitalIoGroup();
	Ui::dioElement *ui;
	QList<QPair<QWidget *,Ui::dioChannel *>*> chui;

Q_SIGNALS:
	void slider(int val);
	/*QList<Ui::dioChannel*> chui;
	QList<QWidget*> chWidget;*/


private Q_SLOTS:
	void on_lineEdit_editingFinished();
	void on_horizontalSlider_valueChanged(int value);
	void on_comboBox_activated(int index);
	void on_inout_clicked();
};

class DigitalIO : public QWidget
{
	friend class DigitalIO_API;
	friend class ToolLauncher_API;

	Q_OBJECT

private:
	Ui::DigitalIO *ui;
	Filter *filt;
	struct iio_context *ctx;
	DigitalIO_API *dio_api;
	QList<DigitalIoGroup *> groups;
	QTimer *poll;
	DIOManager *diom;

	QPair<QWidget *,Ui::dioChannel *>  *findIndividualUi(int ch);

public:
	explicit DigitalIO(struct iio_context *ctx, Filter *filt, QPushButton *runBtn,
	                   DIOManager *diom,
	                   QJSEngine *engine,QWidget *parent = 0);
	~DigitalIO();
	void setDirection(int ch, int direction);
	void setOutput(int ch, int out);

public Q_SLOTS:
	void updateUi();
	void setDirection();
	void setOutput();
	void setSlider(int val);
	void lockUi();
	void on_btnRunStop_clicked();
};

class DigitalIO_API : public ApiObject
{
	Q_OBJECT
	//Q_PROPERTY(QString chm READ chm WRITE setChm SCRIPTABLE false);
	Q_PROPERTY(QList<bool> dir READ direction WRITE setDirection SCRIPTABLE true);
	Q_PROPERTY(QList<bool> out READ output    WRITE setOutput SCRIPTABLE true);


public:
	explicit DigitalIO_API(DigitalIO *dio) : ApiObject(), dio(dio) {}
	~DigitalIO_API() {}

	QList<bool> direction() const;
	void setDirection(const QList<bool>& list);
	QList<bool> output() const;
	void setOutput(const QList<bool>& list);
	void setOutput(int ch, int direction);


private:
	DigitalIO *dio;
};

} /* namespace adiscope */

#endif // DIGITAL_IO_H

