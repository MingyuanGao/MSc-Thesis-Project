cmd_crypto/rng.ko := ld -EL -r  -T /lib/modules/3.8.0-rc4/build/scripts/module-common.lds --build-id  -o crypto/rng.ko crypto/rng.o crypto/rng.mod.o
