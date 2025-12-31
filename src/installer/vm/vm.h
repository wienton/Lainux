#ifndef VM_H
#define VM_H


int check_qemu_dependencies();
void create_virtual_disk();
void install_on_virtual_machine();
void setup_qemu_vm(const char *iso_path);


#endif // vm h
