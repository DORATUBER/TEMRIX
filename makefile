QEMU = /home/shahriyar/qemu/build/qemu-system-x86_64
ASM       = nasm
CC        = g++
EFI_CC    = clang++
LD        = ld
SRC_DIR   = src
BUILD_DIR = build
ASM_DIR   = $(BUILD_DIR)/asm_out
PROGRAMS_DIR := /home/shahriyar/MyOSProjects/C++/TEMRIX4/programs

EFI_BUILD_DIR    = $(BUILD_DIR)/efi
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

BOOTLOADER = $(BUILD_DIR)/BOOTX64.EFI
KERNEL     = $(BUILD_DIR)/kernel.bin
DISK       = disk.img

TEMRIX_GUID = 812daf39-bf7b-4e6d-9acd-886183174a9b

INCLUDES = $(shell find $(SRC_DIR) -type d | sed 's/^/-I/')

CFLAGS = -ffreestanding -m64 -fno-stack-protector -fcf-protection=none \
         -fno-builtin -mno-red-zone -nostdlib -O2 -g \
         -fno-omit-frame-pointer \
         -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
         -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
         -fno-strict-aliasing -fno-delete-null-pointer-checks \
         -std=c++20 \
         $(INCLUDES)

EFI_CFLAGS = --target=x86_64-pc-win32 \
             -ffreestanding -fno-stack-protector \
             -fno-builtin -mno-red-zone -nostdlib -O2 \
             -fno-exceptions -fno-rtti \
             -std=c++20 \
             $(INCLUDES)

EFI_SRCS = $(shell find $(SRC_DIR)/bootloader -name '*.cpp')
EFI_OBJS = $(patsubst $(SRC_DIR)/bootloader/%.cpp, $(EFI_BUILD_DIR)/%.o, $(EFI_SRCS))

KERNEL_SRCS = $(shell find $(SRC_DIR)/kernel -name '*.cpp')
KERNEL_OBJS = $(patsubst $(SRC_DIR)/kernel/%.cpp, $(KERNEL_BUILD_DIR)/%.o, $(KERNEL_SRCS))

ASM_SRCS = $(shell find $(SRC_DIR)/kernel -name '*.asm')
ASM_OBJS = $(patsubst $(SRC_DIR)/kernel/%.asm, $(KERNEL_BUILD_DIR)/%.asm.o, $(ASM_SRCS))

all: setup $(BOOTLOADER) $(KERNEL)

setup:
	@mkdir -p $(BUILD_DIR) $(ASM_DIR)

$(EFI_BUILD_DIR)/boot_stub.o: $(SRC_DIR)/bootloader/boot.asm
	@mkdir -p $(dir $@)
	$(ASM) -f win64 $(INCLUDES) $< -o $@

$(EFI_BUILD_DIR)/%.o: $(SRC_DIR)/bootloader/%.cpp
	@mkdir -p $(dir $@)
	$(EFI_CC) $(EFI_CFLAGS) -c $< -o $@

$(BOOTLOADER): $(EFI_BUILD_DIR)/boot_stub.o $(EFI_OBJS)
	lld-link /subsystem:efi_application /entry:_start /out:$@ $^

$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -mno-sse -mno-sse2 -mno-mmx -mno-80387 -c $< -o $@

$(KERNEL_BUILD_DIR)/%.asm.o: $(SRC_DIR)/kernel/%.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf64 -i$(dir $<) $< -o $@

$(KERNEL): $(KERNEL_OBJS) $(ASM_OBJS)
	$(LD) -T linker.ld -o $(BUILD_DIR)/kernel.elf $(KERNEL_OBJS) $(ASM_OBJS)
	objcopy -O binary $(BUILD_DIR)/kernel.elf $@

$(DISK):
	qemu-img create -f raw $@ 640M
	parted -s $@ mklabel gpt
	sgdisk -n 1:1MiB:511MiB -t 1:$(TEMRIX_GUID) -c 1:"TEMRIX4FS" $@
	sgdisk -n 2:0:639MiB     -t 2:8300           -c 2:"TEMRIX4VOL" $@

	@LODEV=$$(sudo losetup --find --show --partscan $@); \
	sudo mkfs.ext4 $${LODEV}p2; \
	sudo mount $${LODEV}p2 /mnt; \
	sudo mkdir -p /mnt/etc; \
	echo "TEMRIX" | sudo tee /mnt/etc/hostname; \
	sudo mkdir -p /mnt/bin; \
	sudo cp $(PROGRAMS_DIR)/bin/*.trx /mnt/bin/; \
	sudo cp $(PROGRAMS_DIR)/bin/*.trso /mnt/bin/; \
	sudo mkdir -p /mnt/etc; \
	sudo cp $(PROGRAMS_DIR)/etc/init.manifest /mnt/etc/init.manifest; \
	sudo mkdir -p /mnt/home/shahriyar/Downloads; \
	sudo cp /home/shahriyar/Downloads/image.png /mnt/home/shahriyar/Downloads/image.png; \
	sudo cp /home/shahriyar/Downloads/badapple.baa /mnt/home/shahriyar/Downloads/badapple.baa; \
	sudo umount /mnt; \
	sudo losetup -d $${LODEV}

install-programs: all
	@LODEV=$$(sudo losetup --find --show --partscan $(DISK)); \
	sudo mount $${LODEV}p2 /mnt; \
	sudo mkdir -p /mnt/etc; \
	sudo mkdir -p /mnt/bin; \
	sudo cp $(PROGRAMS_DIR)/bin/*.trx /mnt/bin/; \
	sudo cp $(PROGRAMS_DIR)/bin/*.trso /mnt/bin/; \
	sudo mkdir -p /mnt/etc; \
	sudo cp $(PROGRAMS_DIR)/etc/init.manifest /mnt/etc/init.manifest; \
	sudo umount /mnt; \
	sudo losetup -d $${LODEV}

run: all $(DISK)
	mkdir -p $(BUILD_DIR)/esp/EFI/BOOT
	mkdir -p $(BUILD_DIR)/esp/EFI/TEMRIX
	rm -f $(BUILD_DIR)/esp/NvVars
	cp $(BOOTLOADER) $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI
	cp $(BOOTLOADER) $(BUILD_DIR)/esp/EFI/TEMRIX/BOOTX64.EFI
	cp $(KERNEL) $(BUILD_DIR)/esp/EFI/TEMRIX/kernel.bin
	cp $(PROGRAMS_DIR)/bin/Init.trx $(BUILD_DIR)/esp/EFI/TEMRIX/Init.trx
	$(QEMU) \
		-machine q35 \
		-cpu host \
		-enable-kvm \
		-smp 1 \
		-m 256M \
		-bios /usr/share/ovmf/OVMF.fd \
		-vga std \
		-device mygpu \
		-device qemu-xhci,id=xhci \
		-device usb-kbd,bus=xhci.0 \
		-device usb-mouse,bus=xhci.0 \
		-drive file=fat:rw:$(BUILD_DIR)/esp,format=raw \
		-drive file=$(DISK),format=raw,if=none,id=nvme0 \
		-device nvme,drive=nvme0,serial=temrix4 \
		-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
		-device e1000,netdev=net0,mac=52:54:00:12:34:56 \
		-serial mon:stdio \
		-d guest_errors

debug: all $(DISK)
	mkdir -p $(BUILD_DIR)/esp/EFI/BOOT
	mkdir -p $(BUILD_DIR)/esp/EFI/TEMRIX
	rm -f $(BUILD_DIR)/esp/NvVars
	cp $(BOOTLOADER) $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI
	cp $(BOOTLOADER) $(BUILD_DIR)/esp/EFI/TEMRIX/BOOTX64.EFI
	cp $(KERNEL) $(BUILD_DIR)/esp/EFI/TEMRIX/kernel.bin
	qemu-system-x86_64 \
		-machine q35 \
		-cpu host \
		-enable-kvm \
		-smp 1 \
		-m 256M \
		-bios /usr/share/ovmf/OVMF.fd \
		-device qemu-xhci,id=xhci \
		-device usb-kbd,bus=xhci.0 \
		-device usb-mouse,bus=xhci.0 \
		-drive file=fat:rw:$(BUILD_DIR)/esp,format=raw \
		-drive file=$(DISK),format=raw,if=none,id=nvme0 \
		-device nvme,drive=nvme0,serial=temrix4 \
		-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
		-device e1000,netdev=net0,mac=52:54:00:12:34:56 \
		-serial mon:stdio \
		-s -S

clean:
	rm -rf $(BUILD_DIR)

install: all
	sudo mkdir -p /boot/efi/EFI/TEMRIX
	sudo cp $(BOOTLOADER) /boot/efi/EFI/TEMRIX/BOOTX64.EFI
	sudo cp $(KERNEL) /boot/efi/EFI/TEMRIX/kernel.bin
	sudo cp $(PROGRAMS_DIR)/bin/Init.trx /boot/efi/EFI/TEMRIX/Init.trx

boot: install
	sudo efibootmgr --bootnext 0000 && sudo reboot

.PHONY: all setup run debug clean install boot install-programs