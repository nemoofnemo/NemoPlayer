#include "ScreenWidget.h"

using namespace std;

void ScreenWidget::initializeGL(void)
{
	qDebug("ScreenWidget::initializeGL");
	initializeOpenGLFunctions();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

void ScreenWidget::resizeGL(int w, int h)
{
}

void ScreenWidget::paintGL(void)
{
}

ScreenWidget::ScreenWidget(QWidget* parent) : QOpenGLWidget(parent)
{
	
}

ScreenWidget::~ScreenWidget()
{

}

