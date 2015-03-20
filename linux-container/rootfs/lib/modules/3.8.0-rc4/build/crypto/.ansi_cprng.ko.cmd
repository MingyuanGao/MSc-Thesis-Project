cmd_crypto/ansi_cprng.ko := ld -EL -r  -T /lib/modules/3.8.0-rc4/build/scripts/module-common.lds --build-id  -o crypto/ansi_cprng.ko crypto/ansi_cprng.o crypto/ansi_cprng.mod.o
