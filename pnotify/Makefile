TARGET=libpnotify.a
TARGET_DIR=pnotify-0.2

$(TARGET):
	cd $(TARGET_DIR) && ./configure && gmake
	cp $(TARGET_DIR)/$(TARGET) .
	ln -sf $(TARGET_DIR)/pnotify.h

clean:
	rm $(TARGET)
	rm pnotify.h
	cd $(TARGET_DIR) && gmake distclean
