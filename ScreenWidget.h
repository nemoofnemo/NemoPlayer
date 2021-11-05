#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <list>
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMEssageBox>
#include "FFmpegHeader.h"

enum class ScreenStatus {
	SCREEN_STATUS_NONE,
	SCREEN_STATUS_PLAYING,
	SCREEN_STATUS_PAUSE,
	SCREEN_STATUS_HALT
};

class ScreenWidget final : 
	public QOpenGLWidget, 
	protected QOpenGLFunctions_3_3_Core
{
	Q_OBJECT
public:
	struct FrameData {
		uint8_t* data;
		size_t size;
	};

private:
	std::mutex lock;
	int preloadLimit = 10;
	int threadInterval = 1;
	int threadCount = 0;
	int videoWidth = 0;
	int videoHeight = 0;
	ScreenStatus status = ScreenStatus::SCREEN_STATUS_NONE;
	AVHWDeviceType deviceType = AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;
	AVFormatContext* formatContext = nullptr;
	AVCodecContext* videoCodecContext = nullptr;
	AVCodecContext* audioCodecContext = nullptr;
	AVPacket* packet = nullptr;
	std::list<AVFrame*> videoFrameList;
	std::list<AVFrame*> audioFrameList;
	int videoStreamIndex = -1;
	int audioStreamIndex = -1;
	
private:
	int openCodexContext(AVCodecContext** pCC, AVFormatContext* pFC, int index);
	int m_openFile(const QString& path);
	int m_openFileHW(const QString& path);

	//read frame from file.
	static int readThread(ScreenWidget* screen);
	static int decodePacket(ScreenWidget* screen);

	//video display thread
	static int videoThread(ScreenWidget* screen);

protected:
	void initializeGL(void) override;
	void resizeGL(int w, int h) override;
	void paintGL(void) override;

public:
	ScreenWidget() = delete;
	explicit ScreenWidget(QWidget* parent);
	~ScreenWidget();

signals:
	void drawVideoFrame(FrameData data);

private slots:
	void clearOnOpen(void);
	void clearOnClose(void);

public slots:
	void openFile(QString path);
	void close(void);
	void setHWDeviceType(AVHWDeviceType type);
	void test(bool checked);
	void setScreenStatus(ScreenStatus s);
};
