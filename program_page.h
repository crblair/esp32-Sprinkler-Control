#ifndef PROGRAM_PAGE_H
#define PROGRAM_PAGE_H

#include <WebServer.h>
#include "schedule.h"

void handleProgramPage(WebServer& server, ScheduleManager& scheduleManager);

void handleSavePrograms(WebServer& server, ScheduleManager& scheduleManager);

#endif // PROGRAM_PAGE_H
