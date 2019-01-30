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
#include "Context.h"
#include "Athlete.h"
#include "RealtimeData.h"

#include <QMessageBox>
#include <QSerialPort>

DaumController::DaumController(TrainSidebar *parent,  DeviceConfiguration *dc) : RealtimeController(parent, dc) {
    daumDevice_ = new Daum(this, dc != 0 ? dc->portSpec : "", dc != 0 ? dc->deviceProfile : "");
	// set default Mode
	actMode = 1;
}

int DaumController::start() {
//	QDate date;
#ifdef GC_Daum_Debug	
	qDebug() << "start() ";
#endif	
	
//	weight = context->athlete->getWeight(date);
//	height = context->athlete->getHeight();
#ifdef GC_Daum_Debug	
//	qDebug() << "Weigth =" << (int)weight;
//	qDebug() << "Height =" << (int)height;
#endif	
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
#ifdef GC_Daum_Debug
    qDebug() << "actMode =" <<(int)actMode;
#endif
	float aktSlope = (float)rtData.getSlope();

	switch (actMode) {
		case 2 : 
			rtData.setWatts(daumDevice_->getPower());
#ifdef GC_Daum_Debug		
			qDebug() << "Slope=" << aktSlope;
#endif			
			if (oldSlope != aktSlope)  
			  daumDevice_->SetSlope(aktSlope);
			
			break;
		default :
			// Data is set by setload
			rtData.setWatts(daumDevice_->getPower());
			break;
	}		
	oldSlope = aktSlope;
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
#ifdef GC_Daum_Debug	
	qDebug() << "Set mode =" << (int)mode;
#endif	
	actMode = mode;
}
