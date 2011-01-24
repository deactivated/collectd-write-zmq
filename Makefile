ZMQ_ROOT=/hb
COLLECTD_SRC=../collectd-4.10.2
COLLECTD_ROOT=/opt/collectd

LIBTOOL=$(COLLECTD_SRC)/libtool


all: .INIT write_zmq.la

install: all
	$(LIBTOOL) --mode=install /usr/bin/install -c write_zmq.la $(COLLECTD_ROOT)/lib/collectd

clean:
	rm -rf .libs
	rm -rf build
	rm -f write_zmq.la

.INIT:
	mkdir -p build

write_zmq.la: build/write_zmq.lo build/utils_value_json.lo
	$(LIBTOOL) --tag=CC --mode=link gcc -Wall -Werror -O2 -module -avoid-version -L$(ZMQ_ROOT)/lib -o $@ -rpath $(COLLECTD_ROOT)/lib/collectd -lpthread -ldl -lzmq $^

%.lo: ../src/%.c
	$(LIBTOOL) --mode=compile gcc -DHAVE_CONFIG_H -I src -I $(COLLECTD_SRC)/src -I$(ZMQ_ROOT)/include -Wall -Werror -g -O2 -MD -MP -c -o $@ $<