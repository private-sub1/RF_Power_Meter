#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QtWidgets>
#include <QtCore>
#include <QtGui>
#include <QtSerialPort>

namespace Ui {
class MainWidget;
}

struct ModuleParam {
	ModuleParam() = default;
	ModuleParam(quint8 cmd, quint8 size = 1) {
		this->size = size;
		this->cmd = cmd;
	}

	quint8 size;
	quint8 cmd;
};

class Graph : public QWidget
{
	Q_OBJECT
public:
	explicit Graph(QWidget *parent = nullptr);
	void addValue(double dBm);

protected:
	void paintEvent(QPaintEvent *ev) override;
	void resizeEvent(QResizeEvent *ev) override;

private:
	void bufferRepaint();

private:
	QPixmap buffer;
	QList <double> listValues;
};

class MasterThread : public QThread
{
	Q_OBJECT

public:
	explicit MasterThread(QObject *parent = nullptr);
	~MasterThread();

	void openPort(const QString &portName, int portSpeed);
	void closePort();
	void send(const QByteArray &buf);

signals:
	void request(const QByteArray &buf);
	void response(const QByteArray &buf);
	void status(const QString &s, bool res, bool err);

private:
	void run() override;

	QString portName;
	int portSpeed;
	QMutex mutex;
	QWaitCondition condition;
	bool quit = false;
	QQueue <QByteArray> requestBuf;
};

class MainWidget : public QWidget
{
	Q_OBJECT

public:
	explicit MainWidget(QWidget *parent = 0);
	~MainWidget();

protected:
	void closeEvent(QCloseEvent *e);

private slots:
	void showResponse(const QByteArray &buf);
	void on_pbPortOpen_clicked();
	void processStatus(const QString &s, bool res, bool err);
	void on_pbPortAutoRefresh_toggled(bool checked);
	void slotTimerAutoRefresh();
	void slotTimerWaitRecv();
	QByteArray reverse(const QByteArray &buf);


	void on_pbSet_clicked();

private:
	Ui::MainWidget *ui;
	MasterThread thread;
	bool portOpen;
	QTimer timerAutoRefresh;
	QTimer timerWaitRecv;
};



#endif // MAINWIDGET_H
