#include <stddef.h>

const char* ru_strings[][2] = {
    {"WELCOME_TITLE", "Установщик Lainux"},
    {"INSTALL_ON_HARDWARE", "Установить на Железо"},
    {"INSTALL_ON_VM", "Установить в Виртуальную Машину"},
    {"HARDWARE_INFO", "Информация об Оборудовании"},
    {"SYSTEM_REQUIREMENTS", "Проверка Системных Требований"},
    {"CONF_SELECTION", "Выбор Конфигурации"},
    {"DISICK_INFO", "Информация о Дисках"},  // опечатка? пусть будет DISK_INFO
    {"DISK_INFO", "Информация о Дисках"},
    {"NETWORK_CHECK", "Проверка Сети"},
    {"NETWORK_DIAG", "Сетевая Диагностика"},
    {"EXIT_INSTALLER", "Выйти из Установщика"},
    {"PRESS_ANY_KEY", "Нажмите любую клавишу для продолжения..."},
    {"CONFIRM_EXIT", "Выйти из установщика Lainux?"},
    {"TYPE_TO_CONFIRM", "Введите '%s' для подтверждения (ESC — отмена):"},
    {"REBOOT_PROMPT", "Нажмите R, чтобы перезагрузиться сейчас"},
    {"SUMMARY_TARGET", "Цель установки:"},
    {"SUMMARY_CREDENTIALS", "Учётные данные по умолчанию:"},
    {"USERNAME", "Имя пользователя"},
    {"PASSWORD", "Пароль"},
    {"ROOT", "root"},
    {"LAINUX_USER", "lainux"},
    {"REBOOT_WARNING", "⚠ Извлеките установочный носитель перед перезагрузкой!"},
    {"NEXT_STEPS", "Следующие шаги:"},
    {"STEP1", "1. Извлеките установочный носитель"},
    {"STEP2", "2. Перезагрузите систему"},
    {"STEP3", "3. Войдите с указанными учётными данными"},
    {"STEP4", "4. Запустите 'lainux-setup' для настройки"},
    {"NAV_INSTRUCTIONS", "Управление: ↑ ↓ • Выбрать: Enter • Выход: Esc"},
    {"VERSION_INFO", "Версия v0.1 | Поддержка UEFI | Secure Boot"},
    {NULL, NULL}
};
