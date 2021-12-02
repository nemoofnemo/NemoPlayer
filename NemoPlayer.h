#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_NemoPlayer.h"
#include <iostream>
#include <sstream>
#include <string>
#include <list>
#include "FFmpegHeader.h"
#include "DecodeOption.h"

class DecodeOption;

class NemoPlayer : public QMainWindow
{
	Q_OBJECT
private:
	enum class PlayerStatus {
		PLAYER_STATUS_PLAYING,
		PLAYER_STATUS_PAUSE
	};

	Ui::NemoPlayerClass ui;
	std::list<AVHWDeviceType> decodeOptions;
	AVHWDeviceType deviceType = AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;
	PlayerStatus status = PlayerStatus::PLAYER_STATUS_PAUSE;
	

public:
	NemoPlayer(QWidget* parent = Q_NULLPTR);
	~NemoPlayer();

	const std::list<AVHWDeviceType>* getDecodeOptions(void) const{
		return &decodeOptions;
	}

	AVHWDeviceType getCurrentType(void) const {
		return deviceType;
	}

signals:
	

public slots:
	void onDecodeOptionAction(bool checked);
	void onOpenFileAction(bool checked);
	void onCloseAction(bool checked);
	void onSetDeviceType(AVHWDeviceType type);
	void onPlayButtonClicked(bool checked);
};
