#include "DecodeOption.h"

using namespace std;

void DecodeOption::closeEvent(QCloseEvent* event)
{
	const auto optList = mainWindow->getDecodeOptions();
	auto it = btnList.begin();
	auto it2 = optList->begin();
	while (it != btnList.end()) {
		auto ptr = *it;
		if (ptr->isChecked()) {
			mainWindow->set_HW_Type(*it2);
			break;
		}
		it++;
		it2++;
	}
}

DecodeOption::DecodeOption(NemoPlayer* parent) : QDialog(parent)
{
	ui.setupUi(this);
	mainWindow = parent;
	const auto optList = parent->getDecodeOptions();
	for (auto it = optList->begin(); it != optList->end(); it++) {
		auto ptr = new QRadioButton();
		if (*it == AVHWDeviceType::AV_HWDEVICE_TYPE_NONE) {
			ptr->setText("default_cpu");
		}
		else {
			ptr->setText(QString(av_hwdevice_get_type_name(*it)));
		}
		if (*it == mainWindow->getCurrentType()) {
			ptr->setChecked(true);
		}
		btnList.push_back(ptr);
		ui.verticalLayout->addWidget(ptr);
	}
}