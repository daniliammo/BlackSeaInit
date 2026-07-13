#ifndef ПРОЦЕССЫ_H
#define ПРОЦЕССЫ_H

#include <sys/types.h>

typedef struct демон {
    pid_t pid;
    int код_выхода;
    const char *после_успеха;
} демон;

void запустить_процесс(демон *процесс, const char *команда);
void обработать_завершение_процесса(pid_t pid, int статус);

#endif
