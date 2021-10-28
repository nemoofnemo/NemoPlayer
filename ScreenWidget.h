#pragma once
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>

class ScreenWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
	Q_OBJECT
private:
	

protected:
	void initializeGL(void) override;
	void resizeGL(int w, int h) override;
	void paintGL(void) override;

public:
	ScreenWidget() = delete;
	explicit ScreenWidget(QWidget* parent);
	~ScreenWidget();
};
