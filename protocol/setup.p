// Protocol Language

import "sys/virtual"
import "ui"

layer_01 {
    print("Initializing Protocol on Lainux...")

    let target_vm = "qemu-x86_64"

    // Работа с железом через встроенные функции
    if (sys.memory < 2048) {
        ui.warn("Low memory detected for Wired bridge")
    }

    // Блок установки
    accela build_machine {
        iso: "https://dist.lainux.org/latest.iso",
        disk: 20GB,
        virt: target_vm
    }

    print("Connection established.")
}
