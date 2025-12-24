#!/bin/bash
# /usr/local/bin/lainux-lavd-integration.sh

# Lainux LAVD Integration Script
# Поддерживает 4 слоя философии Lainux

case "$1" in
    "layer1")
        # Слой 01: Hardware - диагностика железа
        echo "=== Layer 01: Hardware Diagnostics ==="
        sudo lavdctl metrics show cpu_frequency
        sudo lavdctl metrics show memory_bandwidth
        ;;

    "layer2")
        # Слой 02: Kernel - диагностика ядра
        echo "=== Layer 02: Kernel Diagnostics ==="
        sudo lavdctl metrics show cpu_scheduler
        sudo lavdctl trace --kfunc schedule
        ;;

    "layer3")
        # Слой 03: OpenRC - диагностика инициализации
        echo "=== Layer 03: OpenRC Service Diagnostics ==="
        sudo lavdctl profile --service openrc
        ;;

    "layer4")
        # Слой 04: Userspace - диагностика приложений
        echo "=== Layer 04: Userspace Diagnostics ==="
        sudo lavdctl top --sort cpu
        ;;

    "wired")
        # Полная диагностика системы (WIRED mode)
        echo "=== WIRED: Full System Analysis ==="

        # Сбор всех метрик
        sudo lavdctl diagnose full --output /tmp/lainux-diag-$(date +%s).json

        # Проверка всех слоев
        for layer in {1..4}; do
            echo "Checking Layer $layer..."
            /usr/local/bin/lainux-lavd-integration.sh layer$layer
        done

        echo "=== Let's all love Lain ==="
        ;;

    *)
        echo "Lainux LAVD Integration"
        echo "Usage: $0 {layer1|layer2|layer3|layer4|wired}"
        ;;
esac
