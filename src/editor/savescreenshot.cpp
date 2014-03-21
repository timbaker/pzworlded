#include "savescreenshot.h"

void SaveScreenshot::run(){
	this->pixMap.save(tr("screenshot_%1_%2" ".png").arg(this->x).arg(this->y));
}

