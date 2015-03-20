require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&__ARCH_ARM_POSIX_TYPES_H)) {
    eval 'sub __ARCH_ARM_POSIX_TYPES_H () {1;}' unless defined(&__ARCH_ARM_POSIX_TYPES_H);
    eval 'sub __kernel_mode_t () { &__kernel_mode_t;}' unless defined(&__kernel_mode_t);
    eval 'sub __kernel_ipc_pid_t () { &__kernel_ipc_pid_t;}' unless defined(&__kernel_ipc_pid_t);
    eval 'sub __kernel_uid_t () { &__kernel_uid_t;}' unless defined(&__kernel_uid_t);
    eval 'sub __kernel_old_dev_t () { &__kernel_old_dev_t;}' unless defined(&__kernel_old_dev_t);
    require 'asm-generic/posix_types.ph';
}
1;
