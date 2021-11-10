#pragma once
#include <string>
#include <sstream>
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
		uint8_t* videoData[4] = { NULL };
		int videoLinesize[4] = { 0 };
		int bufSize = 0;
	};

	enum class ThreadStatus {
		THREAD_NONE,
		THREAD_RUN,
		THREAD_PAUSE,
		THREAD_HALT
	};

private:
	std::mutex lock;
	ThreadStatus threadStatus = ThreadStatus::THREAD_NONE;
	ScreenStatus status = ScreenStatus::SCREEN_STATUS_NONE;
	int preloadLimit = 30;
	//time in ms
	int threadInterval = 1;
	int threadCount = 0;
	int videoWidth = 0;
	int videoHeight = 0;
	//time offset from beginning of media file.
	std::chrono::microseconds timeOffset;
	//startTimeStamp will be set with current time
	// when the player start playing or resume playing
	std::chrono::steady_clock::time_point startTimeStamp;
	AVHWDeviceType deviceType = AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;
	AVFormatContext* formatContext = nullptr;
	AVCodecContext* videoCodecContext = nullptr;
	AVCodecContext* audioCodecContext = nullptr;
	AVPacket* packet = nullptr;
	int videoPreload = 60;
	std::mutex videoLock;
	std::list<AVFrame*> videoFrameList;
	int audioPreload = 60;
	std::mutex audioLock;
	std::list<AVFrame*> audioFrameList;
	int videoStreamIndex = -1;
	int audioStreamIndex = -1;
	
	GLuint VBO = 0;
	GLuint VAO = 0;
	GLuint EBO = 0;
	std::string vsCode, fsCode;
	GLuint program = 0;
	GLuint texture = 0;

	float vertices[20] = {
			 1.0f,  1.0f, 0.0f, 1.0f, 0.0f,   // right-top
			 1.0f, -1.0f, 0.0f, 1.0f, 1.0f,   // right-bottom
			-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,   // left-bottom
			-1.0f,  1.0f, 0.0f,  0.0f, 0.0f    // left-top
	};

	unsigned int indices[6] = { 
		0, 1, 3,	//first
		1, 2, 3	//second
	};

private:
	int openCodexContext(AVCodecContext** pCC, AVFormatContext* pFC, int index);
	int m_openFile(const QString& path);
	int m_openFileHW(const QString& path);
	void initShaderScript(void);
	bool createProgram(void);

	//read frame from file.
	static int readThread(ScreenWidget* screen);
	static int decodePacket(ScreenWidget* screen);

	//video display thread
	static int videoThread(ScreenWidget* screen);
	static int m_videoFunc(ScreenWidget* screen, std::chrono::microseconds* time);
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
	void onDrawFrame(FrameData data);

public slots:
	void openFile(QString path);
	void close(void);
	void setHWDeviceType(AVHWDeviceType type);
	void test(bool checked);
	void play(bool checked);
	
};
