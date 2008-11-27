TARGET=libpnotify.a
TARGET_DIR=pnotify-0.2

$(TARGET):
	cd $(TARGET_DIR) && ./configure && gmake
	cp $(TARGET_DIR)/$(TARGET) .

clean:
	rm $(TARGET)
	cd $(TARGET_DIR) && gmake distclean
