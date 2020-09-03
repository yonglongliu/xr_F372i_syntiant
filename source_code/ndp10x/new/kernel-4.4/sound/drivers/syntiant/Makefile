obj-y += ndp10x.o
ndp10x-objs += ndp10x_driver.o syntiant_ndp.o syntiant_ndp10x.o \
	checksum.o syntiant_package.o syntiant_ndp10x_package_parser.o \
	syntiant_ndp_error.o es_conversion.o

# to run SPI at 8MHz, use e.g. following approach
#ccflags-y:= -DSPI_SPLIT=0 -DSPI_READ_DELAY=3 -DMAX_SPEED=8000000

ccflags-y:= -DSPI_SPLIT=0

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean
