#include "NemoAudioDevice.h"

qint64 NemoAudioDevice::bytesAvailable(void) const
{
	return buffer.size() + QIODevice::bytesAvailable();
}

//qint64 NemoAudioDevice::bytesToWrite() const
//{
//	return this->QIODevice::bytesToWrite();
//}

//bool NemoAudioDevice::isSequential(void) const
//{
//	return true;
//}

bool NemoAudioDevice::canReadLine(void) const
{
	return false;
}

qint64 NemoAudioDevice::readData(char* data, qint64 maxSize)
{
	auto ptr = buffer.constData();
	memcpy_s(data, maxSize, ptr, maxSize);
	buffer.remove(0, maxSize);
	return maxSize;
}

qint64 NemoAudioDevice::writeData(const char* data, qint64 maxSize)
{
	buffer.append(data, maxSize);
	return maxSize;
}

NemoAudioDevice::~NemoAudioDevice()
{
	buffer.clear();
	this->close();
}

NemoAudioDevice::NemoAudioDevice(QObject* parent) : QIODevice(parent)
{
	this->open(QIODeviceBase::ReadWrite);
}

void NemoAudioDevice::clear(void)
{
	buffer.clear();
}
