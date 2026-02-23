CC = clang

# --- PATHS ---
SRC_DIR = src

# Added -I. to find headers in your project folder

# NEW ADDED: -Werror -Wextra -03
# 03 - Optimal
# -Werror : will be included
CFLAGS = -Wall -Wextra -O3 -std=c99 -I$(SRC_DIR) -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lraylib -framework IOKit -framework Cocoa -framework OpenGL

OUT = magic
# Source files in src directory, object files in root
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(notdir $(SRCS:.c=.o))

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(LDFLAGS)

# Compile .c files from src directory into .o files in root
%.o: $(SRC_DIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f *.o $(OUT)
