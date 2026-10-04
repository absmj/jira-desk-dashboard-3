#pragma once
#include "../display/IDisplay.h"
#include "Pager.h"
#include "SprintData.h"

// Draws whatever page the pager currently points at (clear + draw + flush).
// `today` comes from the RTC in Phase 3; until then main.cpp supplies a fixed date.
void renderPage(IDisplay& d, const SprintData& data, const Date& today, const Pager& pager);
