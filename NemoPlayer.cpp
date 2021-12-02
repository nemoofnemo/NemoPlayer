#include "NemoPlayer.h"
#include <QFileDialog>
#include <QMessageBox>

NemoPlayer::NemoPlayer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	connect(ui.actionDecodeOption, &QAction::triggered, this, &NemoPlayer::onDecodeOptionAction);
	connect(ui.actionOpen, &QAction::triggered, this, &NemoPlayer::onOpenFileAction);
	connect(ui.actionTest, &QAction::triggered, ui.screen, &ScreenWidget::test);
	connect(ui.playButton, &QPushButton::clicked, this, &NemoPlayer::onPlayButtonClicked);
	connect(ui.actionClose, &QAction::triggered, this, &NemoPlayer::onCloseAction);

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

void NemoPlayer::onDecodeOptionAction(bool checked)
{
	DecodeOption* pDecodeOption = new DecodeOption(this);
	pDecodeOption->exec();
}

void NemoPlayer::onOpenFileAction(bool checked)
{
	QString path = QFileDialog::getOpenFileName(this);
	if (path.length() > 0) {
		ui.screen->openFile(path);
		this->setWindowTitle("NemoPlayer -> " + path);
	}
	else {
		QMessageBox::information(this, "File info", "no file selected.", QMessageBox::StandardButton::Ok);
	}
}

void NemoPlayer::onCloseAction(bool checked)
{
	ui.screen->closeFile();
}

void NemoPlayer::onSetDeviceType(AVHWDeviceType type)
{
	if (this->deviceType != type) {
		QString str = "change device type from "
			+ QString(av_hwdevice_get_type_name(this->deviceType))
			+ " to "
			+ QString(av_hwdevice_get_type_name(type));
		this->deviceType = type;
		ui.screen->setHWDeviceType(type);
		qDebug(str.toStdString().c_str());
	}
}

void NemoPlayer::onPlayButtonClicked(bool checked)
{
	if (status == PlayerStatus::PLAYER_STATUS_PAUSE) {
		ui.screen->play();
		ui.playButton->setText("pause");
		status = PlayerStatus::PLAYER_STATUS_PLAYING;
	}
	else if (status == PlayerStatus::PLAYER_STATUS_PLAYING) {
		ui.screen->pause();
		ui.playButton->setText("play");
		status = PlayerStatus::PLAYER_STATUS_PAUSE;
	}
}
