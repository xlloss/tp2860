KERNELDIR=/media/slash/18t/RZG2L/v3.0/gnkboard-g2l/build_gnk_g2l/linux_kernel_build/kernel-source

obj-m := tp2860.o

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod *.mod.c .tmp_versions modules.order  Module.symvers

.PHONY: modules install clean
