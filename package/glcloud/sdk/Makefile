GL_SRC = $(wildcard ./*.c)
GL_SRC += $(wildcard ./qiniu-c-sdk/*.c)

GL_OBJ  = $(patsubst %.c, %.o, $(GL_SRC))

GL_INC  = -I./ -Iqiniu-c-sdk

CFLAGS  = -Wall
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections

all:
	$(CC) $(CFLAGS) $(GL_SRC) -o glcloud $(GL_INC) libgelian.a libcrypto.a -ldl libcurl.a libssl.a -lpthread -lm -lshare

sinclude .dep
dep:
	$(CC) -MM $(CFLAGS) $(GL_INC) $(GL_SRC) > .dep

clean:  
	rm -f $(GL_OBJ) glcloud


