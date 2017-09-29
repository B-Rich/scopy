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

#ifndef STATISTIC_WIDGET_H
#define STATISTIC_WIDGET_H

#include <QLabel>

namespace Ui {
	class Statistic;
}

namespace adiscope {

class MeasurementData;
class Statistic;
class Formatter;

class StatisticWidget: public QWidget
{
public:
	explicit StatisticWidget(QWidget *parent = nullptr);
	~StatisticWidget();

	QString title() const;
	int channelId() const;
	int positionIndex() const;

	void setTitleColor(const QColor& color);
	void setPositionIndex(int pos);

	void initForMeasurement(const MeasurementData & data);
	void updateStatistics(const Statistic & data);

	void setLineVisible(bool visible);

private:
	Ui::Statistic *m_ui;
	QString m_title;
	int m_channelId;
	int m_posIndex;
	Formatter *m_formatter;
	int m_valueLabelWidth;
};

} // namespace adiscope

#endif // STATISTIC_WIDGET_H
