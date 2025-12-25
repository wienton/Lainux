#!/bin/bash

case "$1" in
    "layer1")
        # Слой 01: Hardware
        echo "*** Layer 01: Hardware Diagnostics ***"
        sudo lavdctl metrics show cpu_frequency
        sudo lavdctl metrics show memory_bandwidth
        ;;

    "layer2")
        # Слой 02: Kernel
        echo "*** Layer 02: Kernel Diagnostics ***"
        sudo lavdctl metrics show cpu_scheduler
        sudo lavdctl trace --kfunc schedule
        ;;

    "layer3")
        # Слой 03: OpenRC
        echo "*** Layer 03: OpenRC Service Diagnostics ***"
        sudo lavdctl profile --service openrc
        ;;

    "layer4")
        # Слой 04: Userspace
        echo "*** Layer 04: Userspace Diagnostics ***"
        sudo lavdctl top --sort cpu
        ;;

    "wired")
        # WIRED mode
        echo "*** WIRED: Full System Analysis ***"

        # Metrics
        sudo lavdctl diagnose full --output /tmp/lainux-diag-$(date +%s).json

        for layer in {1..4}; do
            echo "Checking Layer $layer..."
            /usr/local/bin/lainux-lavd-integration.sh layer$layer
        done

        echo "*** Let's all love Lain ***"
        ;;

    *)
        echo "Lainux LAVD Integration"
        echo "Usage: $0 {layer1|layer2|layer3|layer4|wired}"
        ;;
esac
