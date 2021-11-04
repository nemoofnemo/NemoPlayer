#include "ScreenWidget.h"

using namespace std;

int ScreenWidget::openCodexContext(AVCodecContext** pCC, AVFormatContext* pFC, int index)
{
	int ret = 0;
	const AVCodec* pCodec = NULL;

	if (!pCC || !pFC || index < 0) {
		fprintf(stderr, "Invalid args in openCodexContext");
		return AVERROR(EINVAL);
	}

	/* find decoder for the stream */
	pCodec = avcodec_find_decoder(pFC->streams[index]->codecpar->codec_id);
	if (!pCodec) {
		fprintf(stderr, "Failed to find %s codec\n",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return AVERROR(EINVAL);
	}

	/* Allocate a codec context for the decoder */
	*pCC = avcodec_alloc_context3(pCodec);
	if (!*pCC) {
		fprintf(stderr, "Failed to allocate the %s codec context\n",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return AVERROR(ENOMEM);
	}

	/* Copy codec parameters from input stream to output codec context */
	if ((ret = avcodec_parameters_to_context(*pCC, pFC->streams[index]->codecpar)) < 0) {
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return ret;
	}

	/* Init the decoders */
	if ((ret = avcodec_open2(*pCC, pCodec, NULL)) < 0) {
		fprintf(stderr, "Failed to open %s codec\n",
			av_get_media_type_string(pFC->streams[index]->codecpar->codec_type));
		return ret;
	}

	return 0;
}

void ScreenWidget::clearOnOpen(void)
{
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
	if (videoCodecContext)
		avcodec_free_context(&videoCodecContext);
	if (audioCodecContext)
		avcodec_free_context(&audioCodecContext);
	if (formatContext)
		avformat_close_input(&formatContext);
	videoStreamIndex = -1;
	audioStreamIndex = -1;
}

int ScreenWidget::m_openFile(const QString& path)
{
	int ret = 0;

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
			return ret;
		}
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
			return ret;
		}
	}
	
	return 0;
}

int ScreenWidget::m_openFileHW(const QString& path)
{
	return 0;
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
}

void ScreenWidget::paintGL(void)
{
}

ScreenWidget::ScreenWidget(QWidget* parent) : QOpenGLWidget(parent)
{
	
}

ScreenWidget::~ScreenWidget()
{
	
}

void ScreenWidget::openFile(QString path)
{
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
}

