/*
 * Copyright (c) 2009 Mark Liversedge (liversedge@gmail.com),
 *               2018 Florian Nairz (nairz.florian@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Daum.h"

Daum::Daum(QObject *parent, QString device, QString profile) : QThread(parent),
    timer_(0),
    serialDeviceName_(device),
    serial_dev_(0),
    deviceAddress_(-1),
    maxDeviceLoad_(800),
    paused_(false),
    devicePower_(0),
    deviceHeartRate_(0),
    deviceCadence_(0),
    deviceSpeed_(0),
	num_(1),
	sex_(0),
	height_(175.0),
	weigth_(85.0),
	fat_(0),
	coachGrade_(0),
	coachFreq_(0),
	maxWatt_(0),
	maxPulse_(0),
	maxTime_(0),
	maxDist_(0),
	maxCal_(0),
    load_(kDefaultLoad),
    loadToWrite_(kDefaultLoad),
    forceUpdate_(profile.contains("force", Qt::CaseInsensitive)) {

    this->parent = parent;
}

Daum::~Daum() {}

int Daum::start() {
    QThread::start();
    return isRunning() ? 0 : 1;
}
int Daum::restart() {
    QMutexLocker locker(&pvars);
    paused_ = true;
    return 0;
}
int Daum::pause() {
    QMutexLocker locker(&pvars);
    paused_ = true;
    return 0;
}
int Daum::stop() {
    this->exit(-1);
    return 0;
}

bool Daum::discover(QString dev) {
#ifdef GC_Daum_Debug	
	qDebug() << "discover - device: " << dev;
#endif	
    if (!openPort(dev)) {
		qWarning() << "discover - cannot open device "<< dev; 
        return false;
    }

    QSerialPort &s(*serial_dev_);
    QByteArray data;
	
	// Send Check Cockpit request 
	//   If the cockpit is to old, it does nothing
    data.append((char)0x10);  // Send Check Cockpit
	data.append((char)0x00);  // Is normally ignored by device  

    s.write(data);
    if (!s.waitForBytesWritten(1000)) {
		qWarning() << "check cockpit - timeout send";
		closePort();
        return false;
    }
    if (!s.waitForReadyRead(1000)) {
		qWarning() << "check cockpit - timeout read"; 
		closePort();
        return false;
    }
    data = s.read(3);
    if ((int)data[0] != 0x10) {
		qWarning() << "check cockpit - bad answer";
		closePort();
        return false;
    }
	// clear all data on serial
    data = s.readAll();
    closePort();
#ifdef GC_Daum_Debug
	qDebug() << "discover - ok ";
#endif	
    return true;
}

void Daum::setLoad(double load) {
    const unsigned int minDeviceLoad = 25;
    unsigned int local_load = (unsigned int)load;
    QMutexLocker locker(&pvars);
    if (local_load > maxDeviceLoad_) { local_load = maxDeviceLoad_; }
    if (local_load < minDeviceLoad) { local_load = minDeviceLoad; }
#ifdef GC_Daum_Debug	
    qDebug() << "setLoad(): " << local_load;
#endif	
    loadToWrite_ = local_load;
}

double Daum::getPower() const {
    QMutexLocker locker(&pvars);
    return devicePower_;
}
double Daum::getSpeed() const {
    QMutexLocker locker(&pvars);
    return deviceSpeed_;
}
double Daum::getCadence() const {
    QMutexLocker locker(&pvars);
    return deviceCadence_;
}
double Daum::getHeartRate() const {
    QMutexLocker locker(&pvars);
    return deviceHeartRate_;
}

bool Daum::openPort(QString dev) {
    QMutexLocker locker(&pvars);
    if (serial_dev_ == 0) {
        serial_dev_ = new QSerialPort;
    }
    if (serial_dev_->isOpen()) {
        serial_dev_->close();
    }

    serial_dev_->setPortName(dev);
    serial_dev_->setBaudRate(QSerialPort::Baud9600);
    serial_dev_->setStopBits(QSerialPort::OneStop);
    serial_dev_->setDataBits(QSerialPort::Data8);
    serial_dev_->setFlowControl(QSerialPort::NoFlowControl);
    serial_dev_->setParity(QSerialPort::NoParity);

    if (!serial_dev_->open(QSerialPort::ReadWrite)) {
       return false;
    }

    return true;
}

bool Daum::closePort() {
    QMutexLocker locker(&pvars);
    delete serial_dev_; serial_dev_ = 0;
    return true;
}

void Daum::run() {
    //closePort();
#ifdef GC_Daum_Debug	
	qDebug() << "run ...";
#endif
    if (!openPort(serialDeviceName_)) {
		qWarning() << "run - cannot open device "<< serialDeviceName_; 
        exit(-1);
    }

    {
        QMutexLocker locker(&pvars);
        timer_ = new QTimer();
        if (timer_ == 0) {
			qWarning() << "run - cannot create timer "; 
            exit(-1);
        }
        connect(this, SIGNAL(finished()), timer_, SLOT(stop()), Qt::DirectConnection);
        connect(timer_, SIGNAL(timeout()), this, SLOT(requestRealtimeData()), Qt::DirectConnection);
        // discard prev. read data
        serial_dev_->readAll();
		started_ = true;
    }

    initializeConnection();

    // setup polling
    {
        QMutexLocker locker(&pvars);
        timer_->setInterval(kQueryIntervalMS);
        timer_->start();
    }

    if (!StartProgram(0)) {
        qWarning() << "starting program failed";
    }

    // enter event loop and wait for a call to quit() or exit()
    exec();
	
    if (!StopProgram(0)) {
        qWarning() << "stopping program failed";
    }

    {
        QMutexLocker locker(&pvars);
		started_ = false;
        timer_->stop();
		disconnect(timer_,0,0,0);
    }
#ifdef GC_Daum_Debug	
	qDebug() << "... run finished";
#endif
	
}

void Daum::initializeConnection() {
#ifdef GC_Daum_Debug	
	qDebug() << "initializeConnection ...";
#endif	
    char addr = (char)GetAddress();
    {
        QMutexLocker locker(&pvars);
        deviceAddress_ = addr;
    }
    if (addr < 0) {
        qWarning() << "unable to detect device address - exiting";
        this->exit(-1);
    }

    if (!CheckCockpit()) {
        qWarning() << "Check Cockpit unsuccessfully - exiting";
        this->exit(-1);
    }

    // check version info for know devices
    int dat = GetDeviceVersion();
#ifdef GC_Daum_Debug	
	qDebug() << "GetDeviceVersion() returned " << dat;
#endif	
    switch (dat) {
    case 0x10:				// Cardio
	case 0x1E:				// Update Cockpit Older Version to Vita de Luxe
    case 0x20:				// Fitness
    case 0x30:				// Vita de Luxe
    case 0x40:				// 8008
    case 0x50:				// 8080
    case 0x55:				// ??
    case 0x60:				// Therapy
#ifdef GC_Daum_Debug	
        qDebug() << "Daum cockpit verison: " << dat;
#endif		
        break;
    default:
        qWarning() << "unable to identify daum cockpit version - exiting" << dat;
        this->exit(-1);
        break;  // unreached
    }
    {
        QMutexLocker locker(&pvars);
        bikeType_ = dat;
    }
	// needed if the battery is defect
    if (!SetDate()) {
        qWarning() << "set date failed";
    }
    if (!SetTime()) {
        qWarning() << "set time failed";
    }
	// 
    // reset device
    if (!ResetDevice()) {
        qWarning() << "reset device failed";
    }
	// start programm means user want a new program
    if (!StartProgram(0)) {
        qWarning() << "StartProgram failed";
    }
	// we want programm 0
    if (!SetProgram(0)) {
        qWarning() << "setting program failed";
    }
	// we want person 1
    if (!SetPerson()) {
        qWarning() << "setting person failed";
    }

	// Statparam 
	// 0 = Roadbike
	// 1 = Mountainbike
    if (!SetStartparam(0)) {
        qWarning() << "set startparameters failed";
    }
	// default gear is 10
    if (!SetGear(10)) {
        qWarning() << "set gear failed";
    }

	//
    PlaySound();
}
void Daum::float2Bytes(float val,byte* bytes_array) {
	union {
		float float_variable;
		byte temp_array[4];
	} u;
	// 
	u.float_variable = val;
	//
	memcpy(bytes_array, u.temp_array, 4);
}


void Daum::requestRealtimeData() {
    char addr = -1;
    QByteArray data;
	bool active;
    // Discard any existing data
    {
        QMutexLocker locker(&pvars);
        serial_dev_->readAll();
        addr = deviceAddress_;
		active = started_;
    }
	// if not started the we should do nothing
	if (active != true) {
#ifdef GC_Daum_Debug
    qDebug() << "not started";
#endif
		return;
	}	
#ifdef GC_Daum_Debug
    qDebug() << "querying device info";
#endif
    data.clear();
    data.append((char)0x40);
    data.append(addr);
    data = WriteDataAndGetAnswer(data, 19);
    if (data.length() < 19) {
        return;
    }

    // local cache of telemetry data
	int prg = data[2];							// used programm
	int pers = data[3];							// used person (0=guest,1,2,3,4)
	int pedalling = data[4];   					// either 0/1 or w/ offset of 128
    int pwr = data[5];							// actual power div by 5 = 5-80(25-400W) or 10-160(25 to 800 W)
    int rpm = data[6];							// cyclings (0-199)
    int speed = data[7];						// calculated speed of the bike (0-99km/h), depending of the gear
	int dist = data[8] + 256 * data[9];			// actual distance (in 100m), calculated by bike 
	int pedtime = data[10] + 256 * data[11]; 	// actual cycling time (in sec), calculated by bike  
	int joule = data[12] + 256 * data[13];		// actual used joule (in 100 joule), calculated by bike 
    int pulse = data[14];						// actual pulse (0-199), measured by bike
	int zust = data[15];						// 0= no pulse, 1= to low, 2 = ok, 3 = to high, 4 = blinking, 5 = piep and down the power  
	int gear = data[16];						// actual used gear (1-28)
    int reljoule = data[17] + 256 * data[18];	// used real joule (in 100 joule) person 0=guest is always 0 !
	
	// actual not used information, but collected from Bike
	//   some of the information should transfered to the Controller and 
	//   used for generating the sync with video
	Q_UNUSED(prg);
	Q_UNUSED(pers);
	Q_UNUSED(dist);
	Q_UNUSED(pedtime);
	Q_UNUSED(joule);
	Q_UNUSED(zust);
	Q_UNUSED(gear);
	Q_UNUSED(reljoule);
	
    // sanity check
    if (pwr >= 5 && pwr <= 160) {
        pwr = pwr * 5 * (pedalling != 0 ? 1 : 0);
    } else {
        pwr = 0;
    }
    if (rpm < 0 || rpm > 199) {
        rpm = 0;
    }
    if (speed < 0 || speed > 99) {
        speed = 0;
    }
    if (pulse < 0 || pulse > 199) {
        pulse = 0;
    }

    // assign
    {
        QMutexLocker locker(&pvars);
        devicePower_ = pwr;
        deviceCadence_ = rpm;
        deviceSpeed_ = speed;
        deviceHeartRate_ = pulse;
    }
#ifdef GC_Daum_Debug	
	qDebug() << "pwr=" << (int)pwr;
#endif	
    // write load to device
    bool p = isPaused();
    unsigned int load = kDefaultLoad;
    {
        QMutexLocker locker(&pvars);
        load = load_;
        pwr = loadToWrite_;
		if (pwr < 0) {
#ifdef GC_Daum_Debug			
			qDebug() << "pwr is negativ !!!! " << (int)pwr;
#endif			
			pwr = 0;
		}	
    }
#ifdef GC_Daum_Debug
	qDebug() << "load=" << (int)load << "pwr=" << (int)pwr;
#endif	
    if (!p && (forceUpdate_ || load != pwr)) {
        data.clear();
        data.append((char)0x51);
        data.append(deviceAddress_);
        data.append(MapLoadToByte(pwr));
        qInfo() << "Writing power to device: " << pwr << "W";
        QByteArray res = WriteDataAndGetAnswer(data, 3);
#ifdef GC_Daum_Debug		
        qDebug() << "set power to " << (int)data[2]*5 << "W";
#endif
        if (res != data && res[2] != data[2] && pwr > 400) {
            // reduce power limit because some devices are limited too 400W instead of 800W
            QMutexLocker locker(&pvars);
            maxDeviceLoad_ = 400;
        }
        // update class cache
        {
            QMutexLocker locker(&pvars);
            load_ = pwr;
        }
    }
}

bool Daum::ResetDevice() {
    QByteArray dat;
    dat.append((char)0x12).append(deviceAddress_);
    return WriteDataAndGetAnswer(dat, 3).length() == 2; // device tells pedalling state too
}
int Daum::GetAddress() {
    QByteArray dat;
    dat.append((char)0x11);
    dat = WriteDataAndGetAnswer(dat, 2);
    if (dat.length() == 2 && (int)dat[0] == 0x11) {
        return (int)(dat[1]& 0x00FF);
    }
    return -1;
}
// Fuction 0x10 Check Cockpit
//   do not send answer back if the bike is to old 
bool Daum::CheckCockpit() {
    QByteArray dat;
    dat.append((char)0x10);
    dat.append(deviceAddress_);
    dat = WriteDataAndGetAnswer(dat, 3);
    return (dat.length() == 3 && (int)dat[0] == 0x10);
}
int Daum::GetDeviceVersion() {
    QByteArray dat;
    dat.append((char)0x73);
    dat.append(deviceAddress_);
    dat = WriteDataAndGetAnswer(dat, 11);
    if (dat.length() == 11 && (int)dat[0] == 0x73 && (char)dat[1] == deviceAddress_) {
        return (int)(dat[10] & 0x00FF);
    }
    return -1;
}
bool Daum::SetProgram(unsigned int prog) {
    QByteArray dat;
    if (prog > 79) { prog = 79; }   // clamp to max
    dat.append((char)0x23).append(deviceAddress_).append((char)prog);
    return WriteDataAndGetAnswer(dat, dat.length() + 1).length() == 4; // device tells pedalling state too
}
bool Daum::SetGear(unsigned int gear) {
    QByteArray dat;
    if (gear > 28) { gear = 28; }   // clamp to max
    dat.append((char)0x53).append(deviceAddress_).append((char)gear);
    return WriteDataAndGetAnswer(dat, 3).length() == 3; // 
}
bool Daum::SetStartparam(unsigned int gear) {
    QByteArray dat;
    dat.append((char)0x69).append(deviceAddress_).append((char) 0).append((char) 0).append((char)gear);
    return WriteDataAndGetAnswer(dat, dat.length() + 1).length() == 5; 
}

bool Daum::StartProgram(unsigned int prog) {
    Q_UNUSED(prog);
    QByteArray dat;
    dat.append((char)0x21).append(deviceAddress_);
    return WriteDataAndGetAnswer(dat, dat.length() + 1).length() == 3; // device tells pedalling state too
}
bool Daum::StopProgram(unsigned int prog) {
    Q_UNUSED(prog);
    QByteArray dat;
    dat.append((char)0x22).append(deviceAddress_);
    return WriteDataAndGetAnswer(dat, dat.length() + 1).length() == 3; // device tells pedalling state too
}
bool Daum::SetSlope(float slope) {
	byte bytes[4];
	QByteArray dat;
	
	float2Bytes(slope, &bytes[0]);
	
	dat.append((char)0x55).append(deviceAddress_)
		.append((char)bytes[0])
		.append((char)bytes[1])
		.append((char)bytes[2])
		.append((char)bytes[3]);
    return WriteDataAndGetAnswer(dat, 6).length() == 6;
}
bool Daum::SetDate() {
    QDate d = QDate::currentDate();
    QByteArray dat;
    char year = d.year() - 2000;
    if (year < 0 || year > 99) { year = 0; }
    dat.append((char)0x64).append(deviceAddress_)
            .append((char)d.day())
            .append((char)d.month())
            .append(year);
    return WriteDataAndGetAnswer(dat, 2).length() == 2;
}
bool Daum::SetTime() {
    QTime tim = QTime::currentTime();
    QByteArray dat;
    dat.append((char)0x62).append(deviceAddress_)
            .append((char)tim.second())
            .append((char)tim.minute())
            .append((char)tim.hour());
    return WriteDataAndGetAnswer(dat, 2).length() == 2;
}
bool Daum::SetPerson() {
    QByteArray dat;
    dat.append((char)0x24).append(deviceAddress_)
		.append((char)num_)
		.append((char)old_)
		.append((char)sex_)
		.append((char)height_)
		.append((char)weigth_)
		.append((char)fat_)
		.append((char)coachGrade_)
		.append((char)coachFreq_)
		.append((char)maxWatt_)
		.append((char)maxPulse_)
		.append((char)maxTime_)
		.append((char)maxDist_)
		.append((char)maxCal_);
    dat = WriteDataAndGetAnswer(dat, 16);
    return (dat.length() == 16 && (int)dat[0] == 0x24);
}

void Daum::PlaySound() {
    return;
    // might be buggy in device
    QByteArray dat;
    dat.append((char)0xd3).append((char)   0).append((char)   0);
    WriteDataAndGetAnswer(dat, 0);
}

char Daum::MapLoadToByte(unsigned int load) const {
    char load_map = load/5;
    return load_map;
}

bool Daum::isPaused() const {
    QMutexLocker locker(&pvars);
    return paused_;
}

QByteArray Daum::WriteDataAndGetAnswer(QByteArray const& dat, int response_bytes) {
    QMutexLocker locker(&pvars);

    QSerialPort &s(*serial_dev_);
    QByteArray ret;
    if (!s.isOpen()) {
        return ret;
    }
    s.write(dat);
    if(!s.waitForBytesWritten(1000)) {
        qWarning() << "failed to write data to daum cockpit";
        this->exit(-1);
    }
    if (response_bytes > 0) {
        int retries = 20;
        do {
            if (!s.waitForReadyRead(1000)) {
                return ret;
            }
            ret.append(s.read(response_bytes - ret.length()));
        } while(--retries > 0 && ret.length() < response_bytes);
        if (retries <= 0) {
            qWarning() << "failed to read desired (" << response_bytes << ") data from device. Read: " << ret.length();
            ret.clear();
        }
    }

    s.readAll();    // discard additional data
    return ret;
}
