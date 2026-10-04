#pragma once
#include <FS.h>
#include "SprintData.h"

bool loadSprintFile(fs::FS& fs, const char* path, SprintData& out, const char** err = nullptr);