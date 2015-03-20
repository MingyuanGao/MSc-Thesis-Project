cmd_crypto/krng.ko := ld -EL -r  -T /lib/modules/3.8.0-rc4/build/scripts/module-common.lds --build-id  -o crypto/krng.ko crypto/krng.o crypto/krng.mod.o
