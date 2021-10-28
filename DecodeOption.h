#pragma once
#include <QWidget>
#include <QDialog>
#include <QRadioButton>
#include <list>
#include "NemoPlayer.h"
#include "ui_DecodeOption.h"

class NemoPlayer;

class DecodeOption : public QDialog
{
    Q_OBJECT
private:
    NemoPlayer* mainWindow = nullptr;
    Ui::DecodeOptionWidget ui;
    std::list<QRadioButton*> btnList;

protected:
    void closeEvent(QCloseEvent* event) override;

public:
    DecodeOption() = delete;
    explicit DecodeOption(NemoPlayer* parent);
};

