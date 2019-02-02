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
#include "RideItem.h"
#include "RealtimeData.h"

#include <QMessageBox>
#include <QSerialPort>

DaumController::DaumController(TrainSidebar *parent,  DeviceConfiguration *dc) : RealtimeController(parent, dc) {
    daumDevice_ = new Daum(this, dc != 0 ? dc->portSpec : "", dc != 0 ? dc->deviceProfile : "");

	// 
    this->parent = parent;
#ifdef GC_Daum_Debug	
	qDebug() << "this->parent =" << this->parent;
	if (this->parent != 0) {
		qDebug() << "this->parent->context =" << this->parent->context;
		qDebug() << "this->parent->context->athlete =" << this->parent->context->athlete;
	}
	else {
		qDebug() << "this->parent is nil";
	}
#endif	

	// set defaults 
	actMode = 1;
	weight = 80.0;
	height = 175.0;
	
}

int DaumController::start() {
#ifdef GC_Daum_Debug	
	qDebug() << "start() ";
#endif	
	// Get basicdata for ride
	if (this->parent != 0) {
		weight = this->parent->context->athlete->getWeight(QDate::currentDate());
		height = this->parent->context->athlete->getHeight();
	}
	else {
		qWarning() << "this->parent is nil, use default weight and height";
	}
#ifdef GC_Daum_Debug	
	qDebug() << "Weigth =" << weight; 
	qDebug() << "Height =" << height;
#endif	
    return daumDevice_->start();
}

int DaumController::restart() {
#ifdef GC_Daum_Debug	
	qDebug() << "restart() ";
#endif	
    return daumDevice_->restart();
}

int DaumController::pause() {
#ifdef GC_Daum_Debug	
	qDebug() << "pause() ";
#endif	
    return daumDevice_->pause();
}

int DaumController::stop() {
    return daumDevice_->stop();
}

bool DaumController::discover(QString name) {
#ifdef GC_Daum_Debug	
	qDebug() << "discover() ";
#endif	
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

	float aktSlope = (float)rtData.getSlope();

	switch (actMode) {
		case 2 : 
			rtData.setWatts(daumDevice_->getPower());
#ifdef GC_Daum_Debug		
			qDebug() << "Slope=" << aktSlope << " actMode =" <<(int)actMode;
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
