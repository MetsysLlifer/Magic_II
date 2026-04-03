CC = clang
SRC_DIR = src
CFLAGS = -Wall -O3 -std=c99 -I$(SRC_DIR) -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lraylib -framework IOKit -framework Cocoa -framework OpenGL

OUT = metsys_engine
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/ui.c $(SRC_DIR)/util.c $(SRC_DIR)/npc.c
OBJS = main.o ui.o util.o npc.o

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(LDFLAGS)

%.o: $(SRC_DIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f *.o $(OUT)