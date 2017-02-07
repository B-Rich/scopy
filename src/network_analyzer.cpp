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

#include "network_analyzer.hpp"
#include "signal_generator.hpp"
#include "ui_network_analyzer.h"

#include <gnuradio/analog/sig_source_c.h>
#include <gnuradio/analog/sig_source_f.h>
#include <gnuradio/analog/sig_source_waveform.h>
#include <gnuradio/blocks/complex_to_arg.h>
#include <gnuradio/blocks/complex_to_mag_squared.h>
#include <gnuradio/blocks/float_to_short.h>
#include <gnuradio/blocks/head.h>
#include <gnuradio/blocks/keep_m_in_n.h>
#include <gnuradio/blocks/moving_average_cc.h>
#include <gnuradio/blocks/multiply_cc.h>
#include <gnuradio/blocks/multiply_conjugate_cc.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/null_source.h>
#include <gnuradio/blocks/rotator_cc.h>
#include <gnuradio/blocks/skiphead.h>
#include <gnuradio/blocks/vector_sink_f.h>
#include <gnuradio/blocks/vector_sink_s.h>
#include <gnuradio/top_block.h>

#include <QDebug>
#include <QThread>

#include <iio.h>

/* This should go away ASAP... */
#define DAC_BIT_COUNT   12
#define INTERP_BY_100_CORR 1.168 // correction value at an interpolation by 100
#define AMPLITUDE_VOLTS	5.0

using namespace adiscope;
using namespace gr;

NetworkAnalyzer::NetworkAnalyzer(struct iio_context *ctx,
		Filter *filt, QPushButton *runButton,
		QWidget *parent) : QWidget(parent),
	ui(new Ui::NetworkAnalyzer)
{
	const std::string adc = filt->device_name(TOOL_NETWORK_ANALYZER, 2);
	iio = iio_manager::get_instance(ctx, adc);

	dac1 = filt->find_channel(ctx, TOOL_NETWORK_ANALYZER, 0, true);
	dac2 = filt->find_channel(ctx, TOOL_NETWORK_ANALYZER, 1, true);
	if (!dac1 || !dac2)
		throw std::runtime_error("Unable to find channels in filter file");

	ui->setupUi(this);

	connect(ui->run_button, SIGNAL(toggled(bool)),
			this, SLOT(startStop(bool)));
	connect(ui->run_button, SIGNAL(toggled(bool)),
			runButton, SLOT(setChecked(bool)));
	connect(runButton, SIGNAL(toggled(bool)),
			ui->run_button, SLOT(setChecked(bool)));
	connect(this, &NetworkAnalyzer::sweepDone,
			[=]() {
		ui->run_button->setChecked(false);
	});

	ui->dbgraph->setNumSamples(150);
	ui->dbgraph->setColor(QColor("red"));
	ui->dbgraph->setAxesTitles(QString("Frequency (Hz)"),
			QString("Magnitude (dB)"));

	ui->phasegraph->setNumSamples(150);
	ui->phasegraph->setColor(QColor("blue"));
	ui->phasegraph->setAxesTitles(QString("Frequency (Hz)"),
			QString("Phase (°)"));

	ui->dbgraph->setAxesScales(50e3, 200e3, -40.0, 5.0);
	ui->phasegraph->setAxesScales(50e3, 200e3, -M_PI, M_PI);
}

NetworkAnalyzer::~NetworkAnalyzer()
{
	stop = true;
	thd.waitForFinished();
	delete ui;
}

void NetworkAnalyzer::run()
{
	const struct iio_device *dev = iio_channel_get_device(dac1);

	const struct iio_device *dev1 = iio_channel_get_device(dac1);
	for (unsigned int i = 0; i < iio_device_get_channels_count(dev1); i++) {
		struct iio_channel *each = iio_device_get_channel(dev1, i);

		if (each == dac1 || each == dac2)
			iio_channel_enable(each);
		else
			iio_channel_disable(each);
	}

	const struct iio_device *dev2 = iio_channel_get_device(dac2);
	for (unsigned int i = 0; i < iio_device_get_channels_count(dev2); i++) {
		struct iio_channel *each = iio_device_get_channel(dev2, i);

		if (each == dac1 || each == dac2)
			iio_channel_enable(each);
		else
			iio_channel_disable(each);
	}

	for (unsigned int i = 50; !stop && i < 200; i++) {
		double frequency = i * 1e3;

		unsigned long rate = get_best_sin_sample_rate(dac1, frequency);
		size_t samples_count = get_sin_samples_count(
				dac1, rate, frequency);

		/* We want at least 8 periods. */
		size_t in_samples_count = samples_count * 8;

		if (dev1 != dev2)
			iio_device_attr_write_bool(dev1, "dma_sync", true);

		struct iio_buffer *buf_dac1 = generateSinWave(dev1,
				frequency, rate, samples_count);
		if (!buf_dac1) {
			qCritical() << "Unable to create DAC buffer";
			break;
		}

		struct iio_buffer *buf_dac2 = nullptr;

		if (dev1 != dev2) {
			buf_dac2 = generateSinWave(dev2,
					frequency, rate, samples_count);
			if (!buf_dac2) {
				qCritical() << "Unable to create DAC buffer";
				break;
			}

			iio_device_attr_write_bool(dev1, "dma_sync", false);
		}

		/* Lock the flowgraph if we are already started */
		bool started = iio->started();
		if (started)
			iio->lock();

		/* Skip some data to make sure we'll get the waveform */
		auto skiphead1 = blocks::skiphead::make(sizeof(float),
				in_samples_count);
		auto skiphead2 = blocks::skiphead::make(sizeof(float),
				in_samples_count);
		auto id1 = iio->connect(skiphead1, 0, 0, true,
				in_samples_count);
		auto id2 = iio->connect(skiphead2, 1, 0, true,
				in_samples_count);

		auto f2c1 = blocks::float_to_complex::make();
		auto f2c2 = blocks::float_to_complex::make();
		iio->connect(skiphead1, 0, f2c1, 0);
		iio->connect(skiphead2, 0, f2c2, 0);

		auto null = blocks::null_source::make(sizeof(float));
		iio->connect(null, 0, f2c1, 1);
		iio->connect(null, 0, f2c2, 1);

		/* XXX: FIXME: Don't hardcode the ADC rate */
		auto cosine = analog::sig_source_c::make((unsigned int) 100e6,
				gr::analog::GR_COS_WAVE, -frequency, 1.0);

		auto mult1 = blocks::multiply_cc::make();
		iio->connect(f2c1, 0, mult1, 0);
		iio->connect(cosine, 0, mult1, 1);

		auto mult2 = blocks::multiply_cc::make();
		iio->connect(f2c2, 0, mult2, 0);
		iio->connect(cosine, 0, mult2, 1);

		auto signal = boost::make_shared<signal_sample>();
		auto conj = blocks::multiply_conjugate_cc::make();

		auto avg1 = blocks::moving_average_cc::make(in_samples_count,
				2.0 / in_samples_count, in_samples_count);
		auto keep_one1 = blocks::keep_m_in_n::make(sizeof(gr_complex),
				1, in_samples_count, in_samples_count - 1);
		auto c2m1 = blocks::complex_to_mag_squared::make();

		iio->connect(mult1, 0, avg1, 0);
		iio->connect(avg1, 0, keep_one1, 0);
		iio->connect(keep_one1, 0, c2m1, 0);
		iio->connect(keep_one1, 0, conj, 0);
		iio->connect(c2m1, 0, signal, 0);

		auto avg2 = blocks::moving_average_cc::make(in_samples_count,
				2.0 / in_samples_count, in_samples_count);
		auto keep_one2 = blocks::keep_m_in_n::make(sizeof(gr_complex),
				1, in_samples_count, in_samples_count - 1);
		auto c2m2 = blocks::complex_to_mag_squared::make();

		iio->connect(mult2, 0, avg2, 0);
		iio->connect(avg2, 0, keep_one2, 0);
		iio->connect(keep_one2, 0, c2m2, 0);
		iio->connect(keep_one2, 0, conj, 1);
		iio->connect(c2m2, 0, signal, 1);

		auto c2a = blocks::complex_to_arg::make();
		iio->connect(conj, 0, c2a, 0);
		iio->connect(c2a, 0, signal, 2);

		bool got_it = false;
		float mag1 = 0.0f, mag2 = 0.0f, phase = 0.0f;

		connect(&*signal, &signal_sample::triggered,
				[&](const std::vector<float> values) {
			mag1 = values[0];
			mag2 = values[1];
			phase = values[2];
			got_it = true;
		});

		iio->start(id1);
		iio->start(id2);

		if (started)
			iio->unlock();

		do {
			QThread::msleep(10);
		} while (!got_it);

		iio->stop(id1);
		iio->stop(id2);

		started = iio->started();
		if (started)
			iio->lock();
		iio->disconnect(id1);
		iio->disconnect(id2);
		if (started)
			iio->unlock();

		iio_buffer_destroy(buf_dac1);
		if (buf_dac2)
			iio_buffer_destroy(buf_dac2);


		double mag = 10.0 * log10(mag1) - 10.0 * log10(mag2);
		qDebug() << "Frequency:" << frequency << " Mag diff:"
			<< mag << "Phase diff:" << phase;

		QMetaObject::invokeMethod(ui->dbgraph,
				 "plot",
				 Qt::QueuedConnection,
				 Q_ARG(double, frequency),
				 Q_ARG(double, mag));

		QMetaObject::invokeMethod(ui->phasegraph,
				 "plot",
				 Qt::QueuedConnection,
				 Q_ARG(double, frequency),
				 Q_ARG(double, phase));
	}

	Q_EMIT sweepDone();
}

void NetworkAnalyzer::startStop(bool pressed)
{
	stop = !pressed;

	if (pressed) {
		ui->dbgraph->reset();
		ui->phasegraph->reset();
		thd = QtConcurrent::run(this, &NetworkAnalyzer::run);
	} else {
		thd.waitForFinished();
	}
}

size_t NetworkAnalyzer::get_sin_samples_count(const struct iio_channel *chn,
		unsigned long rate, double frequency)
{
	const struct iio_device *dev = iio_channel_get_device(chn);
	size_t max_buffer_size = 4 * 1024 * 1024 /
		(size_t) iio_device_get_sample_size(dev);
	size_t size = rate / frequency;

	if (size < 2)
		return 0; /* rate too low */

	/* The buffer size must be a multiple of 4 */
	while (size & 0x3)
		size <<= 1;

	/* The buffer size shouldn't be too small */
	while (size < SignalGenerator::min_buffer_size)
		size <<= 1;

	if (size > max_buffer_size)
		return 0;

	return size;
}

unsigned long NetworkAnalyzer::get_best_sin_sample_rate(
		const struct iio_channel *chn, double frequency)
{
	const struct iio_device *dev = iio_channel_get_device(chn);
	QVector<unsigned long> values =
		SignalGenerator::get_available_sample_rates(dev);

	/* Return the best sample rate that we can create a buffer for */
	for (unsigned long rate : values) {
		size_t buf_size = get_sin_samples_count(chn, rate, frequency);
		if (buf_size)
			return rate;

		qDebug() << QString("Rate %1 too high, trying lower")
			.arg(rate);
	}

	throw std::runtime_error("Unable to calculate best sample rate");
}

struct iio_buffer * NetworkAnalyzer::generateSinWave(
		const struct iio_device *dev, double frequency,
		unsigned long rate, size_t samples_count)
{
	/* Create the IIO buffer */
	struct iio_buffer *buf = iio_device_create_buffer(
			dev, samples_count, true);
	if (!buf)
		throw std::runtime_error("Unable to create buffer");

	auto top_block = gr::make_top_block("Signal Generator");

	auto src = analog::sig_source_f::make(rate, analog::GR_SIN_WAVE,
			frequency, 5.0, 0.0);

	// DAC_RAW = (-Vout * 2^11) / 5V
	// Multiplying with 16 because the HDL considers the DAC data as 16 bit
	// instead of 12 bit(data is shifted to the left).
	auto f2s = blocks::float_to_short::make(1,
			-1 * (1 << (DAC_BIT_COUNT - 1)) /
			AMPLITUDE_VOLTS * 16 / INTERP_BY_100_CORR);

	auto head = blocks::head::make(
			sizeof(short), samples_count);

	auto vector = blocks::vector_sink_s::make();

	top_block->connect(src, 0, f2s, 0);
	top_block->connect(f2s, 0, head, 0);
	top_block->connect(head, 0, vector, 0);

	top_block->run();

	const std::vector<short> &samples = vector->data();
	const short *data = samples.data();

	for (unsigned int i = 0; i < iio_device_get_channels_count(dev); i++) {
		struct iio_channel *chn = iio_device_get_channel(dev, i);

		if (iio_channel_is_enabled(chn)) {
			iio_channel_write(chn, buf, data,
					samples_count * sizeof(short));
		}
	}

	iio_device_attr_write_longlong(dev, "sampling_frequency", rate);

	iio_buffer_push(buf);

	return buf;
}