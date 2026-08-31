// #include <csignal>
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
    // Перезапуск делаем в главном цикле (не через после_успеха), поэтому здесь
    // NULL: иначе при выходе shell с кодом 0 запустилось бы ДВА shell'а
    // (один из после_успеха, второй — из главного цикла).
    .после_успеха = NULL,
};

// Команда запуска интерактивного shell. exec — чтобы busybox sh заменил собой
// обёртку «sh -c», и отслеживаемый pid был именно у busybox sh.
static const char *КОМАНДА_ОБОЛОЧКИ = "exec /bin/busybox sh";

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

    запустить_процесс(&оболочка, КОМАНДА_ОБОЛОЧКИ);

    while (1) {
        pause(); // Просыпаемся по любому сигналу (в т.ч. SIGCHLD)

        // Перезапуск shell. Единственный отслеживаемый процесс — оболочка;
        // обработчик SIGCHLD обнуляет её pid при завершении. Если shell вышел
        // (например, пользователь набрал `exit` или процесс упал) — поднимаем
        // заново, как getty. Пробуждения по осиротевшим процессам (их жнёт тот
        // же обработчик) сюда не попадают: там pid оболочки ещё не 0.
        if (оболочка.pid == 0) {
            sleep(1); // анти-busy-loop, если shell падает мгновенно
            запустить_процесс(&оболочка, КОМАНДА_ОБОЛОЧКИ);
        }
    }

    return 0;
}
