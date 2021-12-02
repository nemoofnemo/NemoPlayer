#pragma once
#include <QIODevice>
#include <QByteArray>

class NemoAudioDevice final : public QIODevice
{
	Q_OBJECT
private:
	QByteArray buffer;

public:
	NemoAudioDevice() = delete;
	~NemoAudioDevice();
	explicit NemoAudioDevice(QObject* parent = nullptr);
	NemoAudioDevice(const NemoAudioDevice&) = delete;
	NemoAudioDevice(const NemoAudioDevice&&) = delete;
	NemoAudioDevice& operator=(const NemoAudioDevice&) = delete;

	qint64 bytesAvailable(void) const override;
	bool isSequential(void) const override;
	bool canReadLine(void) const override;
	qint64 readData(char* data, qint64 maxSize);
	qint64 writeData(const char* data, qint64 maxSize);

	void clear(void);
};

