
all:	check

default: check
	
clean: 
	rm -rfv worker master

worker: worker.c
	gcc -o worker worker.c -lm

run-worker: worker
	./worker -x 2 -n 9

master: master.c
	gcc -g -o master master.c

run-master: master
	(sleep 4 && pkill -9 master) &
	./master --worker-path ./worker --wait-mechanism sequential -x 2 -n 9
	(sleep 4 && pkill -9 master) &
	./master --worker-path ./worker --wait-mechanism select -x 2 -n 9
	(sleep 4 && pkill -9 master) &
	./master --worker-path ./worker --wait-mechanism poll -x 2 -n 9
	(sleep 4 && pkill -9 master) &
	./master --worker-path ./worker --wait-mechanism epoll -x 2 -n 9	

check:	clean worker master
		make run-master
		make run-worker

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar