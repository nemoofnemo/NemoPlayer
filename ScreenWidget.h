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
	//int64_t offset = 0;
	//time offset from beginning of media file.
	std::chrono::microseconds timeOffset;
	//startTimeStamp will be set with current time
	// when the player start playing or resume playing
	std::chrono::steady_clock::time_point startTimeStamp;
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
	//audio thread
	static int audioThread(ScreenWidget* screen);
	static int m_audioFunc(ScreenWidget* screen, std::chrono::microseconds* time);

protected:
	void initializeGL(void) override;
	void resizeGL(int w, int h) override;
	void paintGL(void) override;

public:
	ScreenWidget() = delete;
	explicit ScreenWidget(QWidget* parent);
	~ScreenWidget();

	static std::chrono::milliseconds ts_to_millisecond(int64_t ts, int num, int den);
	static std::chrono::microseconds ts_to_microsecond(int64_t ts, int num, int den);

signals:
	void drawVideoFrame(FrameData data);

private slots:
	void clearOnOpen(void);
	void clearOnClose(void);
	void setScreenStatus(ScreenStatus s);

public slots:
	void openFile(QString path);
	void close(void);
	void setHWDeviceType(AVHWDeviceType type);
	void test(bool checked);
	
};
