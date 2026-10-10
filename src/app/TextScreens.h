#pragma once
#include "../display/ITextDisplay.h"
#include "Pager.h"
#include "SprintData.h"

// Same pages as renderPage(), laid out for a character display (16x2 by default).
// Use Pager::setTaskRows(2) so task lists page two rows at a time.
void renderTextPage(ITextDisplay& d, const SprintData& data, const Date& today, const Pager& pager,
                    uint32_t nowMs);

constexpr uint32_t kAlertChunkMs = 2500;  // a long alert message shows two lines at a time, this long each
