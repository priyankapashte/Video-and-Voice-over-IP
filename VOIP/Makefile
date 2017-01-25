GCC_PREFIX=
CFLAGS=

all:receiver_side sender_side

receiver_side: receiver_side.c
	$(GCC_PREFIX)-g++ $(CFLAGS) -o receiver_side.out receiver_side.c -ljpeg pthread asound `pkg-config --cflags opencv` `pkg-config --libs opencv`

sender_side: sender_side.c
	$(GCC_PREFIX)-g++ $(CFLAGS) -o sender_side.out sender_side.c -ljpeg pthread asound `pkg-config --cflags opencv` `pkg-config --libs opencv`

clean: 
	rm -f  receiver_side.out
	rm -f  sender_side.out
