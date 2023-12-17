MODULE = pci_info_v5
USER = user_v5
obj-m += $(MODULE).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod $(MODULE).ko
	make compile

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rmmod $(MODULE)
	rm $(USER)

reload:
	make clean
	make
	make run

cat:
	cat /proc/$(MODULE)


compile:
	gcc $(USER).c -o $(USER)

run:
	sudo ./$(USER)
