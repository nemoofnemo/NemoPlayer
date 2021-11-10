#include "ScreenWidget.h"

using namespace std;

int ScreenWidget::openCodexContext(AVCodecContext** pCC, AVFormatContext* pFC, int index)
{
	int ret = 0;
	const AVCodec* pCodec = NULL;

	if (!pCC || !pFC || index < 0) {
		qDebug("Invalid args in openCodexContext");
		return AVERROR(EINVAL);
	}

	/* find decoder for the stream */
	pCodec = avcodec_find_decoder(pFC->streams[index]->codecpar->codec_id);
	if (!pCodec) {
		qDebug("Failed to find %s codec",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return AVERROR(EINVAL);
	}

	/* Allocate a codec context for the decoder */
	*pCC = avcodec_alloc_context3(pCodec);
	if (!*pCC) {
		qDebug("Failed to allocate the %s codec context",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return AVERROR(ENOMEM);
	}

	/* Copy codec parameters from input stream to output codec context */
	if ((ret = avcodec_parameters_to_context(*pCC, pFC->streams[index]->codecpar)) < 0) {
		qDebug("Failed to copy %s codec parameters to decoder context",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return ret;
	}

	/* Init the decoders */
	if ((ret = avcodec_open2(*pCC, pCodec, NULL)) < 0) {
		qDebug("Failed to open %s codec",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return ret;
	}

	return 0;
}

void ScreenWidget::clearOnOpen(void)
{
	if (packet)
		av_packet_free(&packet);
	if (videoCodecContext)
		avcodec_free_context(&videoCodecContext);
	if (audioCodecContext)
		avcodec_free_context(&audioCodecContext);
	if (formatContext)
		avformat_close_input(&formatContext);
	videoStreamIndex = -1;
	audioStreamIndex = -1;
}

void ScreenWidget::clearOnClose(void)
{
	if (!formatContext) {
		return;
	}

	//wait for threads exit
	lock.lock();
	status = ScreenStatus::SCREEN_STATUS_HALT;
	lock.unlock();
	while (threadCount > 0) {
		this_thread::sleep_for(chrono::milliseconds(threadInterval));
	}

	lock.lock();

	//clear video frame list
	while (videoFrameList.size()) {
		AVFrame* frame = *videoFrameList.begin();
		av_frame_unref(frame);
		av_frame_free(&frame);
		videoFrameList.pop_front();
	}

	//clear audio frame list
	while (audioFrameList.size()) {
		AVFrame* frame = *audioFrameList.begin();
		av_frame_unref(frame);
		av_frame_free(&frame);
		audioFrameList.pop_front();
	}

	videoStreamIndex = -1;
	audioStreamIndex = -1;

	if (packet)
		av_packet_free(&packet);
	if (videoCodecContext)
		avcodec_free_context(&videoCodecContext);
	if (audioCodecContext)
		avcodec_free_context(&audioCodecContext);
	if (formatContext)
		avformat_close_input(&formatContext);

	lock.unlock();
}

int ScreenWidget::m_openFile(const QString& path)
{
	if (formatContext) {
		clearOnClose();
	}

	int ret = 0;
	status = ScreenStatus::SCREEN_STATUS_NONE;

	if ((ret = avformat_open_input(&formatContext, path.toStdString().c_str(), NULL, NULL)) < 0) {
		QMessageBox::critical(nullptr, "error", "cannot open file", QMessageBox::Ok);
		return ret;
	}

	if ((ret = avformat_find_stream_info(formatContext, NULL)) < 0) {
		avformat_close_input(&formatContext);
		QMessageBox::critical(nullptr, "error", "avformat_find_stream_info error", QMessageBox::Ok);
		return ret;
	}

	/* find decoder for the video stream */
	ret = av_find_best_stream(
		formatContext, AVMediaType::AVMEDIA_TYPE_VIDEO,
		-1, -1, NULL, 0);
	if (ret >= 0) {
		videoStreamIndex = ret;
		ret = openCodexContext(&videoCodecContext, formatContext, videoStreamIndex);
		if (ret < 0) {
			clearOnOpen();
			QMessageBox::critical(nullptr, "error", "openCodexContext error", QMessageBox::Ok);
			return ret;
		}
		videoWidth = videoCodecContext->width;
		videoHeight = videoCodecContext->height;
	}

	/* find decoder for the audio stream */
	ret = av_find_best_stream(
		formatContext, AVMediaType::AVMEDIA_TYPE_AUDIO,
		-1, -1, NULL, 0);
	if (ret >= 0) {
		audioStreamIndex = ret;
		ret = openCodexContext(&audioCodecContext, formatContext, audioStreamIndex);
		if (ret < 0) {
			clearOnOpen();
			QMessageBox::critical(nullptr, "error", "openCodexContext error", QMessageBox::Ok);
			return ret;
		}
	}

	packet = av_packet_alloc();
	if (!packet) {
		QMessageBox::critical(nullptr, "error", "av_packet_alloc error", QMessageBox::Ok);
		clearOnOpen();
		ret = AVERROR(ENOMEM);
		return ret;
	}

	timeOffset = chrono::milliseconds(0);

	//start readThread, videoThread, audioThread here
	std::thread t(readThread, this);
	t.detach();

	return 0;
}

int ScreenWidget::m_openFileHW(const QString& path)
{
	return 0;
}

int ScreenWidget::readThread(ScreenWidget* screen)
{
	screen->threadCount++;
	qDebug("readThread start");
	ScreenStatus status = ScreenStatus::SCREEN_STATUS_NONE;
	int ret = 0;
	/*auto funcFlag = [screen]() {
		return (screen->videoFrameList.size() < screen->preloadLimit) || (screen->audioFrameList.size() < screen->preloadLimit);
	};*/
	auto funcFlag = [screen]() {
		return screen->videoFrameList.size() < screen->preloadLimit;
	};

	for (;;) {
		screen->lock.lock();
		status = screen->status;
		screen->lock.unlock();

		if (screen->status == ScreenStatus::SCREEN_STATUS_HALT) {
			break;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PAUSE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_NONE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PLAYING) {
			if (funcFlag()) {
				//read freame here
				if ((ret = av_read_frame(screen->formatContext, screen->packet)) >= 0) {
					ret = avcodec_send_packet(screen->videoCodecContext, screen->packet);
					if ((ret = ScreenWidget::decodePacket(screen)) == 0) {
						av_packet_unref(screen->packet);
						continue;
					}
					else {
						qDebug("decodePacket error");
						av_packet_unref(screen->packet);
						break;
					}
				}
				else {
					//todo: read error.should release resource here
					qDebug("av_read_frame error");
					break;
				}
			}
			else {
				this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			}
		}
		else {
			qDebug("in readThread: fatel error");
			ret = -1;
			break;
		}
	}

	qDebug("readThread done");
	screen->threadCount--;
	return ret;
}

int ScreenWidget::decodePacket(ScreenWidget* screen)
{
	int ret = 0;

	if (screen->packet->stream_index == screen->videoStreamIndex) {
		if ((ret = avcodec_send_packet(screen->videoCodecContext, screen->packet)) < 0) {
			qDebug("avcodec_send_packet error");
			//char log[512] = { 0 };
			//av_strerror(ret, log, 512);
			//qDebug(log);
			return ret;
		}

		while (true) {
			AVFrame* frame = av_frame_alloc();
			if (!frame) {
				qDebug("av_frame_alloc error");
				return -1;
			}

			ret = avcodec_receive_frame(screen->videoCodecContext, frame);
			if (ret < 0) {
				// those two return values are special and mean there is no output
				// frame available, but there were no errors during decoding
				if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
					return 0;
				}
				else {
					qDebug("avcodec_receive_frame error");
					return ret;
				}
			}
			else {
				screen->videoLock.lock();
				screen->videoFrameList.push_back(frame);
				screen->videoLock.unlock();
			}
		}

	}
	else if (screen->packet->stream_index == screen->audioStreamIndex) {

	}
	else {
		return -1;
	}

	return ret;
}

std::chrono::milliseconds ScreenWidget::ts_to_millisecond(int64_t ts, int num, int den)
{
	int64_t arg = 1000 * ts * num / den;
	return std::chrono::milliseconds(arg);
}

std::chrono::microseconds ScreenWidget::ts_to_microsecond(int64_t ts, int num, int den)
{
	int64_t arg = 1000000 * ts * num / den;
	return std::chrono::microseconds(arg);
}

int ScreenWidget::videoThread(ScreenWidget* screen)
{
	screen->threadCount++;
	qDebug("videoThread start");
	int ret = 0;
	int flag = 0;
	chrono::microseconds tmp_time(0);

	for (;;) {
		screen->lock.lock();
		if (screen->status == ScreenStatus::SCREEN_STATUS_HALT) {
			screen->lock.unlock();
			break;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PAUSE) {
			screen->lock.unlock();
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_NONE) {
			screen->lock.unlock();
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PLAYING) {
			if (screen->audioFrameList.size() == 0) {
				screen->lock.unlock();
				this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
				continue;
			}
			else {
				flag = m_videoFunc(screen, &tmp_time);
				screen->lock.unlock();

				if (flag == 1) {
					this_thread::sleep_for(tmp_time);
				}
			}
		}
		else {
			screen->lock.unlock();
			qDebug("in videoThread: fatel error");
			exit(-1);
		}
	}

	qDebug("readThread done");
	screen->threadCount--;
	return ret;
}

int ScreenWidget::m_videoFunc(ScreenWidget* screen, std::chrono::microseconds* time)
{
	int ret = 0;

	while (screen->audioFrameList.size() > 0) {
		auto frame = *(screen->audioFrameList.begin());
		auto dt = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - screen->startTimeStamp);
		auto target_time = screen->timeOffset + dt;
		chrono::microseconds t1(ts_to_microsecond(
			frame->pts,
			screen->audioCodecContext->time_base.num,
			screen->audioCodecContext->time_base.den
		));
		chrono::microseconds t2(ts_to_microsecond(
			frame->pts + frame->pkt_duration,
			screen->audioCodecContext->time_base.num,
			screen->audioCodecContext->time_base.den
		));

		if (target_time >= t1 && target_time < t2) {
			FrameData data;
			SwsContext* sws_ctx = sws_getContext(
				frame->width, frame->height, (AVPixelFormat)frame->format,
				frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_RGB24,
				SWS_BILINEAR, NULL, NULL, NULL);

			if (!sws_ctx) {
				return -1;
			}

		}
		else if (target_time < t1) {
			*time = t1 - target_time;
			return 1;
		}
		else {
			av_frame_unref(frame);
			av_frame_free(&frame);
			screen->audioFrameList.pop_front();
			continue;
		}
	}

	return ret;
}

int ScreenWidget::audioThread(ScreenWidget* screen)
{
	screen->threadCount++;
	qDebug("audioThread start");
	int ret = 0;
	int flag = 0;
	chrono::microseconds tmp_time(0);

	for (;;) {
		screen->lock.lock();
		if (screen->status == ScreenStatus::SCREEN_STATUS_HALT) {
			screen->lock.unlock();
			break;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PAUSE) {
			screen->lock.unlock();
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_NONE) {
			screen->lock.unlock();
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (screen->status == ScreenStatus::SCREEN_STATUS_PLAYING) {
			if (screen->audioFrameList.size() == 0) {
				screen->lock.unlock();
				this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
				continue;
			}

			flag = m_audioFunc(screen, &tmp_time);
			screen->lock.unlock();

			//todo
		}
		else {
			screen->lock.unlock();
			qDebug("in videoThread: fatel error");
			exit(-1);
		}
	}

	qDebug("audioThread done");
	screen->threadCount--;
	return ret;
}

int ScreenWidget::m_audioFunc(ScreenWidget* screen, std::chrono::microseconds* time)
{
	int ret = 0;

	while (screen->audioFrameList.size() > 0) {
		auto frame = *(screen->audioFrameList.begin());
		auto dt = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - screen->startTimeStamp);
		auto target_time = screen->timeOffset + dt;
		chrono::microseconds t1(ts_to_microsecond(
			frame->pts,
			screen->audioCodecContext->time_base.num,
			screen->audioCodecContext->time_base.den
		));
		chrono::microseconds t2(ts_to_microsecond(
			frame->pts + frame->pkt_duration,
			screen->audioCodecContext->time_base.num,
			screen->audioCodecContext->time_base.den
		));

		if (target_time >= t1 && target_time < t2) {

		}
		else if (target_time < t1) {
			*time = t1 - target_time;
			return 1;
		}
		else {
			av_frame_unref(frame);
			av_frame_free(&frame);
			screen->audioFrameList.pop_front();
			continue;
		}
	}

	return ret;
}

void ScreenWidget::initializeGL(void)
{
	qDebug("ScreenWidget::initializeGL");
	initializeOpenGLFunctions();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

void ScreenWidget::resizeGL(int w, int h)
{
	qDebug("resizeGL: w=%d, h=%d", w, h);
}

void ScreenWidget::paintGL(void)
{
}

ScreenWidget::ScreenWidget(QWidget* parent) : QOpenGLWidget(parent)
{
	timeOffset = chrono::microseconds(0);
	startTimeStamp = chrono::steady_clock::now();
}

ScreenWidget::~ScreenWidget()
{
	clearOnClose();
}

void ScreenWidget::openFile(QString path)
{
	if (path.size() == 0) {
		QMessageBox::information(this, "open file", "invalid path", QMessageBox::Ok);
	}

	if (deviceType == AVHWDeviceType::AV_HWDEVICE_TYPE_NONE) {
		if (m_openFile(path) == 0) {
			QMessageBox::information(this, "open file", path, QMessageBox::Ok);
		}
		else {

		}
	}
	else {
		if (m_openFileHW(path) == 0) {
			QMessageBox::information(this, "open file. use hardware accelerate", path, QMessageBox::Ok);
		}
		else {

		}
	}
}

void ScreenWidget::close(void)
{
}

void ScreenWidget::setHWDeviceType(AVHWDeviceType type)
{
	deviceType = type;
}

void ScreenWidget::test(bool checked)
{
	qDebug("test triggered");
}

void ScreenWidget::play(bool checked)
{
	if (formatContext) {
		setScreenStatus(ScreenStatus::SCREEN_STATUS_PLAYING);
	}
}

void ScreenWidget::setScreenStatus(ScreenStatus s)
{
	qDebug("setScreenStatus: %d to %d", status, s);

	lock.lock();
	status = s;
	if (s == ScreenStatus::SCREEN_STATUS_PLAYING) {
		startTimeStamp = chrono::steady_clock::now();
	}
	lock.unlock();

	if (s == ScreenStatus::SCREEN_STATUS_HALT) {
		clearOnClose();
	}
}

