# Copyright (c) 2024 Miguel "Peppermint" Robledo
#
# SPDX-License-Identifier: 0BSD

SRC = hash.c
OBJS = $(SRC:.c=.o)
EXE = $(SRC:.c=)

CFLAGS += \
	-Wall -Werror -pedantic -O3 -g \
	$(shell pkg-config --cflags OpenCL)
LDFLAGS += \
	$(shell pkg-config --libs OpenCL)

all: $(EXE)

clean:
	rm -f $(EXE) $(OBJS)

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

deps:
	@[ "$$(id -u)" -eq 0 ] || \
		(echo "You must be root to install dependencies" && exit 1)
	zypper install -y ocl-icd-devel
	# apt install -u ocl-icd-dev
