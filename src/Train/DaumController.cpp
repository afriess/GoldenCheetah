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

#include "DaumController.h"
#include "Daum.h"
#include "RealtimeData.h"

#include <QMessageBox>
#include <QSerialPort>

DaumController::DaumController(TrainSidebar *parent,  DeviceConfiguration *dc) : RealtimeController(parent, dc) {
    daumDevice_ = new Daum(this, dc != 0 ? dc->portSpec : "", dc != 0 ? dc->deviceProfile : "");
	// set default Mode
	actMode = 1;
}

int DaumController::start() {
    return daumDevice_->start();
}

int DaumController::restart() {
    return daumDevice_->restart();
}

int DaumController::pause() {
    return daumDevice_->pause();
}

int DaumController::stop() {
    return daumDevice_->stop();
}

bool DaumController::discover(QString name) {
   return daumDevice_->discover(name);
}

/*
 * gets called from the GUI to get updated telemetry.
 * so whilst we are at it we check button status too and
 * act accordingly.
 */
void DaumController::getRealtimeData(RealtimeData &rtData) {
    if(!daumDevice_->isRunning()) {
        QMessageBox msgBox;
        msgBox.setText(tr("Cannot Connect to Daum"));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        parent->Stop(1);
        return;
    }

    qDebug() << "actMode =" <<(int)actMode;

	// Calculation simplified from http://www.kreuzotter.de/deutsch/speed.htm
	double V = daumDevice_->getSpeed() * 0.27778;
	double slope = atan(rtData.getSlope()) * 0.01;
	// Weight bike + person = 120.0 :-)
	double wght = 120.0;
	double Frg = 9.81 * (wght) * (0.0046  * cos(slope) + sin(slope)); 
	double P = 1.025 * V * (0.3165 * (V * V) + Frg + V * 0.1);
	if (P<=25) {
		// Downhill is negative power, set to 25 to avoid problems
		P = 25.0;
	};	
	
	switch (actMode) {
		case 2 : 
			qDebug() << "P load=" << (double)P << "V corr in=" << (double)V << "Slope corr in=" << (double)slope;
			// 
			P = round(P / 5)  * 5; 
	
			daumDevice_->setLoad(P);
			rtData.setWatts(P);
			break;
		default :
			// Data is set by setload
			rtData.setWatts(daumDevice_->getPower());
			break;
	}		
    rtData.setHr(daumDevice_->getHeartRate());
    rtData.setCadence(daumDevice_->getCadence());
    rtData.setSpeed(daumDevice_->getSpeed());
}

void DaumController::setLoad(double load) {
	switch (actMode) {
		case 2 : 
		    // Not allowed, set by getrealtimedata
			break;
		default:
			daumDevice_->setLoad(load);
			break;
	}
}

void DaumController::setMode(int mode)
{
	// mode 1 = ERG     (= Watt)
	// mode 2 = SLOPE   (= slope %) 
	qDebug() << "Set mode =" << (int)mode;
	actMode = mode;
}
