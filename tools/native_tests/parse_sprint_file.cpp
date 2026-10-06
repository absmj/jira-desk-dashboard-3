// Parses a sprint.json produced by the web app with the SAME loader the firmware uses.
//   parse_sprint_file <file>   -> prints a summary, exit code 1 if the device would reject it
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/SprintLoader.h"

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: parse_sprint_file <file>\n"); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    static char buf[16384];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    static SprintData s;
    const char* err = nullptr;
    if (!parseSprintJson(buf, s, &err)) { printf("REJECTED: %s\n", err ? err : "?"); return 1; }
    printf("OK bytes=%zu name=%s done=%u/%u tasks=%u blocked=%u overdue=%u top=%s(%u) pace=%d\n", n, s.name,
           s.done, s.total, s.taskCount, s.stats.blocked, s.stats.overdue, s.stats.topName, s.stats.topCount,
           static_cast<int>(s.stats.pace));
    for (uint8_t i = 0; i < s.taskCount; ++i) printf("  %-8s %s  %-3s %s\n", s.tasks[i].key, s.tasks[i].status, "", s.tasks[i].title);
    return 0;
}
