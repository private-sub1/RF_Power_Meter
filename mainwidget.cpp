#include "mainwidget.h"
#include "ui_mainwidget.h"

#define FILENAME_SETTINGS "/settings.ini"


MainWidget::MainWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MainWidget)
{
	ui->setupUi(this);

	ui->cbSpeed->addItem("1200", QSerialPort::Baud1200);
	ui->cbSpeed->addItem("2400", QSerialPort::Baud2400);
	ui->cbSpeed->addItem("4800", QSerialPort::Baud4800);
	ui->cbSpeed->addItem("9600", QSerialPort::Baud9600);
	ui->cbSpeed->addItem("19200", QSerialPort::Baud19200);
	ui->cbSpeed->addItem("38400", QSerialPort::Baud38400);
	ui->cbSpeed->addItem("57600", QSerialPort::Baud57600);
	ui->cbSpeed->addItem("115200", QSerialPort::Baud115200);

	connect(&timerAutoRefresh, &QTimer::timeout, this, &MainWidget::slotTimerAutoRefresh);
	connect(&timerWaitRecv, &QTimer::timeout, this, &MainWidget::slotTimerWaitRecv);

	connect(&thread, &MasterThread::response, this, &MainWidget::showResponse);
//	connect(&thread, &MasterThread::request, this, &MainWidget::showRequest);
	connect(&thread, &MasterThread::status, this, &MainWidget::processStatus);
	portOpen = false;

	slotTimerAutoRefresh();


	QSettings set(qApp->applicationDirPath() + FILENAME_SETTINGS, QSettings::IniFormat, this);
	restoreGeometry(set.value("Geometry", QByteArray()).toByteArray());
	ui->cbPort->setCurrentText(set.value("Port", "").toString());
	ui->cbSpeed->setCurrentIndex(set.value("Speed", 0).toInt());
	ui->pbPortAutoRefresh->setChecked(set.value("PortAutoRefresh", true).toBool());
	ui->sbCor->setValue(set.value("Cor", 0.0).toDouble());
	ui->sbFreq->setValue(set.value("Freq", 3000).toInt());
	ui->rbMinus->setChecked(set.value("Plus", false).toBool());
}

MainWidget::~MainWidget()
{
	delete ui;
}

void MainWidget::closeEvent(QCloseEvent *e)
{
	QSettings set(qApp->applicationDirPath() + FILENAME_SETTINGS, QSettings::IniFormat, this);
	set.setValue("Geometry", saveGeometry());
	set.setValue("Port", ui->cbPort->currentText());
	set.setValue("Speed", ui->cbSpeed->currentIndex());
	set.setValue("PortAutoRefresh", ui->pbPortAutoRefresh->isChecked());
	set.setValue("Cor", ui->sbCor->value());
	set.setValue("Freq", ui->sbFreq->value());
	set.setValue("Plus", ui->rbPlus->isChecked());

	QWidget::closeEvent(e);
}

void MainWidget::on_pbPortOpen_clicked()
{
	ui->pbPortOpen->setEnabled(false);
	if (portOpen) {
		thread.closePort();
	} else {
		thread.openPort(ui->cbPort->currentText(), ui->cbSpeed->currentData().toInt());
	}
}

void MainWidget::processStatus(const QString &s, bool res, bool err)
{
	if (err) {
		ui->lStatus->setStyleSheet(".QLabel { background-color : #ec7070; }");
	} else {
		ui->lStatus->setStyleSheet("");
	}
	ui->lStatus->setText(tr("%1").arg(s));
	portOpen = res;
	if (res) {
		ui->pbPortOpen->setText("Close");
		timerWaitRecv.start(2000);
		ui->lStatus->setStyleSheet(".QLabel { background-color : #70ec70; }");
	} else {
		ui->pbPortOpen->setText("Open");
		timerWaitRecv.stop();
		ui->wMeasurement->setStyleSheet("");
	}
	ui->pbPortOpen->setEnabled(true);
	ui->pbPortAutoRefresh->setEnabled(!portOpen);
	ui->cbPort->setEnabled(!portOpen);
	ui->cbSpeed->setEnabled(!portOpen);
}

void MainWidget::on_pbPortAutoRefresh_toggled(bool checked)
{
	if (checked) {
		slotTimerAutoRefresh();
		timerAutoRefresh.start(2000);
	} else {
		timerAutoRefresh.stop();
	}
}

void MainWidget::slotTimerAutoRefresh()
{
	if (portOpen) {
		return;
	}
	const auto infos = QSerialPortInfo::availablePorts();
	for (const QSerialPortInfo &info : infos) {
		bool find = false;
		for (int i = 0; i < ui->cbPort->count(); ++i) {
			if (ui->cbPort->itemText(i) == info.portName()) {
				find = true;
				break;
			}
		}
		if (!find) {
			ui->cbPort->addItem(info.portName());
			ui->cbPort->setCurrentIndex(ui->cbPort->count() - 1);
		}
	}
	for (int i = 0; i < ui->cbPort->count(); ++i) {
		bool find = false;
		for (const QSerialPortInfo &info : infos) {
			if (ui->cbPort->itemText(i) == info.portName()) {
				find = true;
				break;
			}
		}
		if (!find) {
			ui->cbPort->removeItem(i--);
		}
	}
}


QByteArray MainWidget::reverse(const QByteArray &buf)
{
	QByteArray res;
	for (auto item : buf) {
		res.prepend(item);
	}
	return res;
}

void MainWidget::slotTimerWaitRecv()
{
	ui->wMeasurement->setStyleSheet(".QWidget { background-color : #ec7070; }");
}

void MainWidget::showResponse(const QByteArray &buf)
{
	//qWarning() << QString(buf.toHex(' ')).toUpper();
	//qWarning() << QString(buf);
	QString text = QString(buf);
	QRegularExpression reDbm("\\$([\\s\\S]+?)dBm");
	QRegularExpressionMatch matchDbm = reDbm.match(text);
	QStringList listDbm = matchDbm.capturedTexts();
	if (!listDbm.isEmpty()) {
		QString dBm = listDbm.last().remove(' ');
		dBm = dBm.replace(':', "10").replace(';', "11").
		      replace('<', "12").replace('=', "13").
		      replace('>', "14").replace('?', "15").
		      replace('@', "16");
		bool ok;
		double val = dBm.toDouble(&ok);
		QString dBmStr = dBm.replace('.', ',');
		ui->lStatus1->setText(dBmStr + " dBm");
		if (ok) {
			ui->wGraph->addValue(val);
			double power = pow(10.0, val / 10.0) / 1000.0;
			QString mW = QString::number(power, 'f', 1).replace('.', ',');
			ui->lStatus2->setText(mW + " W");
			timerWaitRecv.start(1000);
			ui->wMeasurement->setStyleSheet("");
		}
	}
	QRegularExpression reMVpp("dBm([\\s\\S]+?)\\$");
	QRegularExpressionMatch matchMVpp = reMVpp.match(text);
	QStringList listMVpp = matchMVpp.capturedTexts();
	if (!listMVpp.isEmpty()) {
		QString mVpp = listMVpp.last().remove(' ');
		mVpp.insert(mVpp.length() - 4, ' ');
		ui->lStatus3->setText(mVpp);
	}
}

void MainWidget::on_pbSet_clicked()
{
	thread.send(QString("$%1%2%3#").
	              arg(ui->sbFreq->value(), 4, 10, QChar('0')).
	              arg(ui->rbPlus->isChecked() ? "+" : "-").
	              arg(ui->sbCor->value(), 4, 'f', 1, QChar('0')).toLatin1());
}

MasterThread::MasterThread(QObject *parent) :
	QThread(parent)
{
}

MasterThread::~MasterThread()
{
	mutex.lock();
	quit = true;
	condition.wakeOne();
	mutex.unlock();
	wait();
}

void MasterThread::openPort(const QString &portName, int portSpeed)
{
	const QMutexLocker locker(&mutex);
	this->portName = portName;
	this->portSpeed = portSpeed;
	if (!isRunning()) {
		start();
	} else {
		condition.wakeOne();
	}
}

void MasterThread::closePort()
{
	const QMutexLocker locker(&mutex);
	this->portName.clear();
	if (isRunning()) {
		condition.wakeOne();
	}
}

void MasterThread::send(const QByteArray &buf)
{
	const QMutexLocker locker(&mutex);
	if (requestBuf.size() < 500) {
		if (isRunning()) {
			requestBuf.enqueue(buf);
			condition.wakeOne();
		}
	}

}

void MasterThread::run()
{
	QSerialPort serial;
	QByteArray currentRequest;
	QString currentPortName;
	int currentPortSpeed = 0;
	int needCheck = 0;
	while (!quit) {
		mutex.lock();
		if (requestBuf.isEmpty()) {
			condition.wait(&mutex, 50);
		}
		bool currentPortChanged = false;
		if (currentPortName != portName) {
			currentPortName = portName;
			currentPortChanged = true;
		}
		if (currentPortSpeed != portSpeed) {
			currentPortSpeed = portSpeed;
			currentPortChanged = true;
		}
		if (currentPortChanged) {
			requestBuf.clear();
		}
		if (!requestBuf.isEmpty()) {
			currentRequest = requestBuf.dequeue();
		} else {
			currentRequest.clear();
		}

		mutex.unlock();
		if (currentPortChanged) {
			if (serial.isOpen()) {
				serial.close();
			}
			if (currentPortName.isEmpty()) {
				emit status(tr("Port closed"), false, false);
				return;
			}
			serial.setPortName(currentPortName);
			serial.setBaudRate(currentPortSpeed);
			if (!serial.open(QIODevice::ReadWrite)) {
				emit status(tr("Can't open %1, error code %2")
				           .arg(currentPortName).arg(serial.error()), false, true);
				return;
			} else {
				emit status(tr("Port %1 opened").arg(currentPortName), true, false);
			}
		}

		if (!currentRequest.isEmpty()) {
			// write request
			serial.write(currentRequest);
			if (serial.waitForBytesWritten(100)) {
				emit request(currentRequest);
			}
		}
		// read response
		if (serial.waitForReadyRead(100)) {
			needCheck = 0;
			QByteArray responseData = serial.readAll();
			while (serial.waitForReadyRead(50) && responseData.size() < 200) {
				responseData += serial.readAll();
			}
			emit this->response(responseData);
		} else {
			needCheck++;
			if (needCheck > 50) {
				needCheck = 0;
				bool portDisconnect = true;
				const auto infos = QSerialPortInfo::availablePorts();
				for (const QSerialPortInfo &info : infos) {
					if (info.portName() == currentPortName) {
						portDisconnect = false;
						break;
					}
				}
				if (portDisconnect) {
					if (serial.isOpen()) {
						serial.close();
					}
					emit status(tr("Port disconnected"), false, false);
					return;
				}
			}
		}
	}
}




Graph::Graph(QWidget* parent) :
    QWidget(parent)
{

}

void Graph::addValue(double dBm)
{
	listValues.append(dBm);
	if (listValues.size() > 1000) {
		listValues.removeFirst();
	}
	bufferRepaint();
	repaint();
}

void Graph::paintEvent(QPaintEvent* ev)
{
	Q_UNUSED(ev)
	QPainter p(this);
	p.drawPixmap(0, 0, buffer);
}

void Graph::resizeEvent(QResizeEvent* ev)
{
	Q_UNUSED(ev)
	buffer = QPixmap(width(), height());
	bufferRepaint();
}

void Graph::bufferRepaint()
{
	QPainter p(&buffer);
	auto w = buffer.width();
	auto h = buffer.height();
	p.fillRect(0, 0, w, h, QBrush(QColor(100, 100, 100)));

	if (listValues.size() > 2) {
		double max, min;
		max = min = listValues.first();
		for (auto i = 1; i < listValues.size(); ++i) {
			auto v = listValues.at(i);
			if (max < v) {
				max = v;
			}
			if (min > v) {
				min = v;
			}
		}
		if (max < 0) {
			max = 1.0;
		}
		if (min > 0) {
			min = -1.0;
		}
		auto scale = h / (max - min);
		auto count = listValues.size();
		auto offset = count - w;
		if (offset < 0) {
			offset = 0;
		}
		p.setPen(QColor(150, 150, 255));
		for (auto x = 1; x < w && x < count; ++x) {
			auto y0 = h - (listValues.at(offset + x - 1) - min) * scale;
			auto y1 = h - (listValues.at(offset + x) - min) * scale;
			p.drawLine(x, y0, x + 1, y1);
		}
	}

}
