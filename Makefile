

.PHONY: all clean codec

all: codec image_dataset/gray8bit get_peak_error test

codec:
	make -C codec

get_peak_error:
	make -C tools/get_peak_error


image_dataset:
	mkdir image_dataset

image_dataset/gray8bit.zip:  | image_dataset
	cd image_dataset; \
	wget https://imagecompression.info/test_images/gray8bit.zip

image_dataset/gray8bit:  image_dataset/gray8bit.zip
	cd image_dataset; \
	unzip gray8bit.zip -d gray8bit

clean:
	rm image_dataset/gray8bit.zip

distclean:
	rm -rf image_dataset
	make -C tools/get_peak_error clean
	make -C codec clean

test: codec get_peak_error image_dataset/gray8bit
	bash ./Test/quick_test.sh