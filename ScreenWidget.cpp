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
	if (frame)
		av_frame_free(&frame);
	if (packet)
		av_packet_free(&packet);
	if (audioSink) {
		audioSink->stop();
		delete audioSink;
		audioSink = nullptr;
	}
	if (audioDevice) {
		delete audioDevice;
		audioDevice = nullptr;
	}
	if (audioFormat) {
		delete audioFormat;
		audioFormat = nullptr;
	}
	if (swr_ctx) {
		swr_free(&swr_ctx);
		swr_ctx = nullptr;
	}
	if (sws_ctx) {
		sws_freeContext(sws_ctx);
		sws_ctx = nullptr;
	}
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

	lock.lock();
	status = ScreenStatus::SCREEN_STATUS_HALT;
	readStatus = ThreadStatus::THREAD_HALT;
	lock.unlock();

	//wait for all work threads exit
	while (threadCount > 0) {
		this_thread::sleep_for(chrono::milliseconds(10));
	}

	//clear video frame list
	while (videoFrameList.size()) {
		auto it = videoFrameList.begin();
		av_freep(&(it->videoData[0]));
		videoFrameList.pop_front();
	}

	//todo: flush decoder

	if (frame)
		av_frame_free(&frame);
	if (packet)
		av_packet_free(&packet);
	if (audioSink) {
		audioSink->stop();
		delete audioSink;
		audioSink = nullptr;
	}
	if (audioDevice) {
		delete audioDevice;
		audioDevice = nullptr;
	}
	if (audioFormat) {
		delete audioFormat;
		audioFormat = nullptr;
	}
	if (swr_ctx) {
		swr_free(&swr_ctx);
		swr_ctx = nullptr;
	}
	if (sws_ctx) {
		sws_freeContext(sws_ctx);
		sws_ctx = nullptr;
	}
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
	if (formatContext) {
		clearOnClose();
	}

	int ret = 0;

	readStatus = ThreadStatus::THREAD_NONE;
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

		sws_ctx = sws_getContext(
			videoWidth, videoHeight, videoCodecContext->pix_fmt,
			videoWidth, videoHeight, AVPixelFormat::AV_PIX_FMT_RGB24,
			SWS_BILINEAR, NULL, NULL, NULL);

		if (!sws_ctx) {
			QMessageBox::critical(nullptr, "error", "sws_getContext error", QMessageBox::Ok);
			clearOnOpen();
			return -1;
		}
	}
	else {
		qDebug("no video");
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

		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			QMessageBox::critical(nullptr, "error", "Could not allocate resampler context", QMessageBox::Ok);
			clearOnOpen();
			return AVERROR(ENOMEM);
		}

		audioChannels =
			av_get_channel_layout_nb_channels(audioCodecContext->channel_layout);

		av_opt_set_int(swr_ctx, "in_channel_layout", audioCodecContext->channel_layout, 0);
		av_opt_set_int(swr_ctx, "in_sample_rate", audioCodecContext->sample_rate, 0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audioCodecContext->sample_fmt, 0);

		av_opt_set_int(swr_ctx, "out_channel_layout", audioCodecContext->channel_layout, 0);
		av_opt_set_int(swr_ctx, "out_sample_rate", audioSampleRate, 0);
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", audioFromat, 0);

		if ((ret = swr_init(swr_ctx)) < 0) {
			QMessageBox::critical(nullptr, "error", "openCodexContext error", QMessageBox::Ok);
			clearOnOpen();
			return ret;
		}

		audioFormat = new QAudioFormat;
		audioFormat->setSampleRate(audioSampleRate);
		audioFormat->setChannelCount(audioChannels);
		audioFormat->setSampleFormat(QAudioFormat::SampleFormat::Float);
		audioSink = new QAudioSink(*audioFormat, nullptr);
		audioDevice = new NemoAudioDevice(nullptr);
		audioSink->start(audioDevice);
		audioSink->suspend();
	}
	else {
		qDebug("no audio");
	}

	packet = av_packet_alloc();
	if (!packet) {
		QMessageBox::critical(nullptr, "error", "av_packet_alloc error", QMessageBox::Ok);
		clearOnOpen();
		ret = AVERROR(ENOMEM);
		return ret;
	}

	frame = av_frame_alloc();
	if (!frame) {
		QMessageBox::critical(nullptr, "error", "av_frame_alloc error", QMessageBox::Ok);
		clearOnOpen();
		ret = AVERROR(ENOMEM);
		return ret;
	}

	//start readThread, videoThread, audioThread here
	std::thread t(readThread, this);
	t.detach();

	if(videoCodecContext){
		std::thread t2(videoThread, this);
		t2.detach();
	}

	/*if (audioCodecContext) {
		std::thread t3(audioThread, this);
		t3.detach();
	}*/

	return 0;
}

int ScreenWidget::m_openFileHW(const QString& path)
{
	return 0;
}

void ScreenWidget::initShaderScript(void)
{
	stringstream vs, fs;

	vs << "#version 330 core" << endl
		<< "layout(location = 0) in vec3 aPos;" << endl
		<< "layout(location = 1) in vec2 aTexCoord;" << endl
		<< "out vec2 optTexCoord;" << endl
		<< "void main()" << endl
		<< "{" << endl
		<< "gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);" << endl
		<< "optTexCoord = aTexCoord;" << endl
		<< "}" << endl;
	vsCode = vs.str();

	fs << "#version 330 core" << endl
		<< "in vec2 optTexCoord;" << endl
		<< "out vec4 FragColor;" << endl
		<< "uniform sampler2D texture0;" << endl
		<< "void main()" << endl
		<< "{" << endl
		<< "FragColor = texture(texture0, optTexCoord);" << endl
		<< "}" << endl;
	fsCode = fs.str();
}

bool ScreenWidget::createProgram(void)
{
	initShaderScript();

	const char* vertexShaderSource = vsCode.c_str();
	const char* fragmentShaderSource = fsCode.c_str();
	int  success = 0;
	char infoLog[512] = { '\0' };

	auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		qDebug("ERROR::vertexShader::COMPILATION_FAILED");
		qDebug(infoLog);
		return false;
	}

	//fragment shader
	auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		qDebug("ERROR::fragmentShader::COMPILATION_FAILED");
		qDebug(infoLog);
		return false;
	}

	//link shader program
	program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		qDebug("ERROR::shaderProgram::FAILED");
		qDebug(infoLog);
		return false;
	}

	//release shaders after link
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	qDebug("createProgram done");
	return true;
}

int ScreenWidget::readThread(ScreenWidget* screen)
{
	screen->threadCount++;
	
	ThreadStatus status = ThreadStatus::THREAD_NONE;
	int ret = 0;

	auto funcFlag = [screen]() {
		if (screen->audioCodecContext && screen->videoCodecContext) {
			return (screen->videoFrameList.size() < screen->videoPreload);
		}
		else if (screen->audioCodecContext && !screen->videoCodecContext) {
			return true;
		}
		else if (!screen->audioCodecContext && screen->videoCodecContext) {
			return screen->videoFrameList.size() < screen->videoPreload;
		}
		else {
			return false;
		}
	};

	qDebug("readThread start");

	for (;;) {
		screen->lock.lock();
		status = screen->readStatus;
		screen->lock.unlock();

		if (status == ThreadStatus::THREAD_HALT) {
			break;
		}
		else if (status == ThreadStatus::THREAD_PAUSE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (status == ThreadStatus::THREAD_NONE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (status == ThreadStatus::THREAD_RUN) {
			//read frame here
			if (funcFlag()) {
				if (av_read_frame(screen->formatContext, screen->packet) == 0) {
					if (screen->packet->stream_index == screen->videoStreamIndex) {
						ret = decodeVideo(screen);
					}
					else if (screen->packet->stream_index == screen->audioStreamIndex) {
						ret = decodeAudio(screen);
					}
					else {
						ret = -1;
					}
					av_packet_unref(screen->packet);
					if (ret < 0) {
						this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
					}
				}
				else {
					this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
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

	screen->threadCount--;
	qDebug("readThread done");
	return ret;
}

int ScreenWidget::decodeVideo(ScreenWidget* screen)
{
	int ret = avcodec_send_packet(screen->videoCodecContext, screen->packet);
	if (ret < 0) {
		qDebug("video avcodec_send_packet error");
		//char log[512] = { 0 };
		//av_strerror(ret, log, 512);
		//qDebug(log);
		return -1;
	}

	while (true) {
		auto frame = screen->frame;

		ret = avcodec_receive_frame(screen->videoCodecContext, frame);
		if (ret < 0) {
			// those two return values are special and mean there is no output
			// frame available, but there were no errors during decoding
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
				return 0;
			}
			else {
				qDebug("video avcodec_receive_frame error");
				return -1;
			}
		}

		VideoData data;
		data.bufSize = av_image_alloc(
			data.videoData, data.videoLinesize,
			frame->width, frame->height,
			AVPixelFormat::AV_PIX_FMT_RGB24, 1);

		if (data.bufSize < 0) {
			qDebug("video av_image_alloc error");
			av_frame_unref(frame);
			return -1;
		}

		sws_scale(screen->sws_ctx, (const uint8_t* const*)frame->data,
			frame->linesize, 0, frame->height, data.videoData, data.videoLinesize);

		data.pts = chrono::microseconds(
			ts_to_microsecond(frame->pts,
				screen->formatContext->streams[screen->videoStreamIndex]->time_base)
		);
		data.duration = chrono::microseconds(
			ts_to_microsecond(frame->pkt_duration,
				screen->formatContext->streams[screen->videoStreamIndex]->time_base)
		);

		av_frame_unref(frame);

		screen->videoLock.lock();
		screen->videoFrameList.push_back(data);
		screen->videoLock.unlock();
	}

	return 0;
}

int ScreenWidget::decodeAudio(ScreenWidget* screen)
{
	int ret = avcodec_send_packet(screen->audioCodecContext, screen->packet);
	if (ret < 0) {
		qDebug("audio avcodec_send_packet error: %d", ret);
		return -1;
	}

	while (true) {
		auto frame = screen->frame;
		ret = avcodec_receive_frame(screen->audioCodecContext, frame);
		if (ret < 0) {
			// those two return values are special and mean there is no output
			// frame available, but there were no errors during decoding
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
				return 0;
			}
			else {
				qDebug("audio avcodec_receive_frame error");
				return -1;
			}
		}

		uint8_t** dst_data = nullptr;
		int dst_linesize = 0;
		auto dst_nb_samples = av_rescale_rnd(
			frame->nb_samples,
			//swr_get_delay(screen->swr_ctx, screen->audioCodecContext->sample_rate) + frame->nb_samples,
			screen->audioSampleRate, frame->sample_rate, AV_ROUND_UP);
		/*auto dst_nb_channels =
			av_get_channel_layout_nb_channels(screen->audioCodecContext->channel_layout);*/

		ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize,
			screen->audioChannels, dst_nb_samples,
			screen->audioFromat, 0);
		if (ret < 0) {
			av_frame_unref(frame);
			qDebug("audio av_samples_alloc_array_and_samples error");
			return -1;
		}

		ret = swr_convert(screen->swr_ctx, dst_data, dst_nb_samples,
			(const uint8_t**)frame->data, frame->nb_samples);
		if (ret < 0) {
			av_frame_unref(frame);
			if (dst_data) {
				av_freep(&dst_data[0]);
				av_freep(&dst_data);
			}
			qDebug("audio swr_convert error");
			return -1;
		}

		auto dst_bufsize = av_samples_get_buffer_size(&dst_linesize, 
			screen->audioChannels,
			ret, screen->audioFromat, 1);
		if (dst_bufsize < 0) {
			av_frame_unref(frame);
			if (dst_data) {
				av_freep(&dst_data[0]);
				av_freep(&dst_data);
			}
			qDebug("audio av_samples_get_buffer_size error");
			return -1;
		}
		
		/*AudioData data;
		data.audioData = dst_data[0];
		data.bufSize = dst_bufsize;
		data.pts = chrono::microseconds(
			ts_to_microsecond(frame->pts,
				screen->formatContext->streams[screen->audioStreamIndex]->time_base));
		data.duration = chrono::microseconds(
			ts_to_microsecond(frame->pkt_duration,
				screen->formatContext->streams[screen->audioStreamIndex]->time_base));

		screen->audioLock.lock();
		screen->audioFrameList.push_back(data);
		screen->audioLock.unlock();*/

		screen->audioDevice->write((char*)dst_data[0], dst_bufsize);
		av_freep(&dst_data[0]);

		av_frame_unref(frame);
		if (dst_data) {
			//av_freep(&dst_data[0]);
			av_freep(&dst_data);
		}
	}

	return 0;
}

std::chrono::milliseconds ScreenWidget::ts_to_millisecond(int64_t ts, AVRational time_base)
{
	int64_t arg = 1000 * ts * time_base.num / time_base.den;
	return std::chrono::milliseconds(arg);
}

std::chrono::milliseconds ScreenWidget::ts_to_millisecond(int64_t ts, int num, int den)
{
	int64_t arg = 1000 * ts * num / den;
	return std::chrono::milliseconds(arg);
}

std::chrono::microseconds ScreenWidget::ts_to_microsecond(int64_t ts, AVRational time_base)
{
	int64_t arg = 1000000 * ts * time_base.num / time_base.den;
	return std::chrono::microseconds(arg);
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
	ScreenStatus status = ScreenStatus::SCREEN_STATUS_NONE;
	chrono::microseconds tmp_time(0);

	for (;;) {
		screen->lock.lock();
		status = screen->status;
		screen->lock.unlock();

		if (status == ScreenStatus::SCREEN_STATUS_HALT) {
			break;
		}
		else if (status == ScreenStatus::SCREEN_STATUS_PAUSE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (status == ScreenStatus::SCREEN_STATUS_NONE) {
			this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
			continue;
		}
		else if (status == ScreenStatus::SCREEN_STATUS_PLAYING) {
			if (screen->videoFrameList.size() == 0) {
				this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
				continue;
			}
			else {
				flag = m_videoFunc(screen, &tmp_time);
				if (flag == 1) {
					this_thread::sleep_for(tmp_time);
				}
				else {
					this_thread::sleep_for(chrono::milliseconds(screen->threadInterval));
				}
			}
		}
		else {
			qDebug("in videoThread: fatel error");
			ret = -1;
			break;
		}
	}

	screen->threadCount--;
	qDebug("videoThread done");
	return ret;
}

int ScreenWidget::m_videoFunc(ScreenWidget* screen, std::chrono::microseconds* time)
{
	int ret = 0;
	
	screen->videoLock.lock();
	while (screen->videoFrameList.size()) {
		auto it = screen->videoFrameList.begin();
		auto dt = chrono::duration_cast<chrono::microseconds>(
			chrono::steady_clock::now() - screen->startTimeStamp);
		auto current = screen->timeOffset + dt;
		auto t1 = it->pts;
		auto t2 = t1 + it->duration;

		if (current >= t1 && current < t2) {
			emit screen->drawVideoFrame(*it);
			screen->videoFrameList.pop_front();
			*time = t2 - current;
			ret = 1;
			break;
		}
		else if (current < t1) {
			*time = t1 - current;
			ret = 1;
			break;
		}
		else {
			emit screen->drawVideoFrame(*it);
			screen->videoFrameList.pop_front();
			continue;
		}
	}
	screen->videoLock.unlock();

	return ret;
}

void ScreenWidget::initializeGL(void)
{
	initializeOpenGLFunctions();

	//compile and link program
	createProgram();

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// texture
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glUniform1i(glGetUniformLocation(program, "texture0"), 0);
	glActiveTexture(GL_TEXTURE0);

	qDebug("ScreenWidget::initializeGL done");
}

void ScreenWidget::resizeGL(int w, int h)
{
	qDebug("resizeGL: w=%d, h=%d", w, h);
}

void ScreenWidget::paintGL(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	// bind textures on corresponding texture units
	glBindTexture(GL_TEXTURE_2D, texture);
	glUseProgram(program);

	// render container
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void ScreenWidget::onDrawFrame(VideoData data)
{
	makeCurrent();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		videoWidth, videoHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data.videoData[0]);
	// paintGL();
	av_freep(&data.videoData[0]);
	update();
}

void ScreenWidget::onUpdateScreen(void)
{
	update();
}

ScreenWidget::ScreenWidget(QWidget* parent) : QOpenGLWidget(parent)
{
	timeOffset = chrono::microseconds(0);
	startTimeStamp = chrono::steady_clock::now();

	connect(this, &ScreenWidget::drawVideoFrame, this, &ScreenWidget::onDrawFrame);
	connect(this, &ScreenWidget::updateScreen, this, &ScreenWidget::onUpdateScreen);

	}

ScreenWidget::~ScreenWidget()
{
	clearOnClose();
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}

void ScreenWidget::openFile(QString path)
{
	clearScreen();

	if (path.size() == 0) {
		QMessageBox::information(this, "open file", "invalid path", QMessageBox::Ok);
	}

	if (deviceType == AVHWDeviceType::AV_HWDEVICE_TYPE_NONE) {
		if (m_openFile(path) == 0) {
			//QMessageBox::information(this, "open file", path, QMessageBox::Ok);
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

void ScreenWidget::closeFile(void)
{
	clearOnClose();
	clearScreen();
}

void ScreenWidget::setHWDeviceType(AVHWDeviceType type)
{
	deviceType = type;
}

void ScreenWidget::test(bool checked)
{
	qDebug("test triggered");
	//QFile* sourceFile = new QFile;   // class member.
	//QAudioSink* audio; // class member.
	//sourceFile->setFileName("D:/mov/audioTest");
	//sourceFile->open(QIODevice::ReadOnly);

	//QAudioFormat format;
	//// Set up the format, eg.
	//format.setSampleRate(48000);
	//format.setChannelCount(1);
	//format.setSampleFormat(QAudioFormat::Float);

	//audio = new QAudioSink(format, this);
	//audio->start(sourceFile);
}

void ScreenWidget::play(bool checked)
{
	if (formatContext) {
		setScreenStatus(ScreenStatus::SCREEN_STATUS_PLAYING);
	}
}

void ScreenWidget::clearScreen(void)
{
	makeCurrent();
	uint8_t arr[3] = { 0 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, arr);
	update();
}

void ScreenWidget::setScreenStatus(ScreenStatus s)
{
	qDebug("setScreenStatus: %d to %d", status, s);

	if (s == ScreenStatus::SCREEN_STATUS_PLAYING) {
		lock.lock();
		readStatus = ThreadStatus::THREAD_RUN;
		lock.unlock();

		qDebug("waiting for preload.");
		int cnt = 0;
		while (cnt < 20 && videoFrameList.size() == 0 ) {
			this_thread::sleep_for(chrono::milliseconds(50));
			cnt++;
		}

		lock.lock();
		status = ScreenStatus::SCREEN_STATUS_PLAYING;
		startTimeStamp = chrono::steady_clock::now();
		lock.unlock();

		audioSink->resume();
	}
	
	if (s == ScreenStatus::SCREEN_STATUS_HALT) {
		clearOnClose();
	}
}



