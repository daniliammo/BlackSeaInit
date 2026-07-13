#include <csignal>
#define _GNU_SOURCE
#include <bits/types/siginfo_t.h>
#include <stdio.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fstab.h>

#include "процессы.h"


static int время_ожидания_завершения = 3;

static демон оболочка = {
    .pid = 0,
    .код_выхода = -1,
    .после_успеха = "/bin/busybox sh",
};

void обработчик_выключения(int сигнал, siginfo_t *информация, void *контекст)
{
    uid_t uid_пользователя = информация->si_uid;
    if (uid_пользователя != 0) { // 0 - uid суперпользователя
        return;
    }

    if (сигнал == SIGUSR1 || сигнал == SIGUSR2 || сигнал == SIGTERM || сигнал == SIGPWR){
        kill(-1, SIGTERM); // -1 Значит завершение всех процессов кроме себя через вежливое завершение сигнал 15
        sleep(время_ожидания_завершения);
        kill(-1, SIGKILL); // Завершение всех процессов через сигнал 9, принудительное завершение.

        sync();

        switch (сигнал) {
            case SIGUSR1:
                reboot(RB_HALT_SYSTEM); // Halt

            case SIGUSR2:
                reboot(RB_POWER_OFF); // Выключение

            case SIGTERM:
                reboot(RB_AUTOBOOT); // Перезагрузка

            case SIGPWR:
                reboot(RB_POWER_OFF); // Выключение
        }
    }
}

void обработчик_зомби_процессов(int сигнал) {
    int статус;
    pid_t завершившийся;

    while ((завершившийся = waitpid(-1, &статус, WNOHANG)) > 0) {
        обработать_завершение_процесса(завершившийся, статус);
    }
}

void примонтировать_основные_фс() {
    int результат = system("mount -a");

    if (результат == 0) {
        printf("Инициализация: Все файловые системы успешно примонтированы.\n");
    } else {
        fprintf(stderr, "Инициализация: Ошибка при монтировании. Код: %d\n", результат);
    }
}

void назначить_сигналы() {
    signal(SIGCHLD, обработчик_зомби_процессов);

    struct sigaction sa;

    sa.sa_sigaction = обработчик_выключения; // Указываем расширенный обработчик
    sigemptyset(&sa.sa_mask);           // Очищаем маску блокируемых сигналов
    sa.sa_flags = SA_SIGINFO;            // Ключевой флаг для получения данных

    // Регистрируем сигналы
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPWR, &sa, NULL);
}

int main() {
    if (getpid() != 1){
        printf("PID процесса не равен 1. Завершение.");
        return 1;
    }

    примонтировать_основные_фс();

	назначить_сигналы();

    запустить_процесс(&оболочка, "/bin/busybox sh");

    while (1) {
        pause(); // Ожидание сигналов
    }

    return 0;
}
