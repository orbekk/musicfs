SUBDIRS= src
TARGET= musicfs

.PHONY: all
all: $(TARGET)

# A bit of a hack..
$(TARGET): $(SUBDIRS)
	cp src/$(TARGET) .

.PHONY: subdirs $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do ($(MAKE) -C $$d clean); done
	rm -f $(TARGET)
