OVERLAY=dt_liam

dtb:
	sudo cp $(PWD)/uEnv.txt /boot/
	dtc -I dts -O dtb -o $(OVERLAY).dtb $(OVERLAY).dts
	sudo cp $(PWD)/$(OVERLAY).dtb /boot/dtbs/4.19.280-bone76/
