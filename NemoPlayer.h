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
    Ui::NemoPlayerClass ui;
    std::list<AVHWDeviceType> decodeOptions;
    AVHWDeviceType type = AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;

public:
    NemoPlayer(QWidget *parent = Q_NULLPTR);
    ~NemoPlayer();

    const std::list<AVHWDeviceType>& getDecodeOptions(void) {
        return decodeOptions;
    }

    AVHWDeviceType getCurrentType(void) {
        return type;
    }

    void set_HW_Type(AVHWDeviceType type);

public slots:
    void onDecodeOptionAction(bool checked);
};
