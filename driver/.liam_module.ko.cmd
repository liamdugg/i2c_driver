cmd_/home/ubuntu/i2c_driver/driver/liam_module.ko := ld -r  -EL -z noexecstack  -T ./scripts/module-common.lds -T ./arch/arm/kernel/module.lds  --build-id  -o /home/ubuntu/i2c_driver/driver/liam_module.ko /home/ubuntu/i2c_driver/driver/liam_module.o /home/ubuntu/i2c_driver/driver/liam_module.mod.o ;  true
