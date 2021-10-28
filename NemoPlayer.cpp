#include "NemoPlayer.h"

NemoPlayer::NemoPlayer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	connect(ui.actionDecodeOption, &QAction::triggered, this, &NemoPlayer::onDecodeOptionAction);

	qDebug("ScreenWidget::ScreenWidget");
	AVHWDeviceType type = AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;
	qDebug("Available device types:");
	decodeOptions.push_back(AVHWDeviceType::AV_HWDEVICE_TYPE_NONE);
	while ((type = av_hwdevice_iterate_types(type)) != AVHWDeviceType::AV_HWDEVICE_TYPE_NONE) {
		qDebug(" %s", av_hwdevice_get_type_name(type));
		decodeOptions.push_back(type);
	}
}

NemoPlayer::~NemoPlayer()
{

}

void NemoPlayer::set_HW_Type(AVHWDeviceType type)
{
	if (this->type != type) {
		QString str = "change device type from "
			+ QString(av_hwdevice_get_type_name(this->type))
			+ " to "
			+ QString(av_hwdevice_get_type_name(type));
		this->type = type;
		qDebug(str.toStdString().c_str());
	}
	return;
}

void NemoPlayer::onDecodeOptionAction(bool checked)
{
	DecodeOption* pDecodeOption = new DecodeOption(this);
	pDecodeOption->exec();
}
