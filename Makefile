#CFLAGS= -g -Wall -Iinclude -MMD
#CC=gcc
#MOD_DIR=./modules
#SRC_DIR=./source
#DEP_DIR := deps
#DEPFILES := $(wildcard $(DEP_DIR)/*.d)
##DEPFLAGS = -MT -MMD -MP -MF $(DEPDIR)/$*.d
#MODS = $(patsubst %.c,%.o,$(wildcard $(MOD_DIR)/*.c))
#SRCS := $(wildcard $(SRC_DIR)/*.c)
##DEPFILES := $(SRCS:%.c=$(DEP_DIR)/%.d)
#
##OBJ= $(patsubst %.c,%.o,$(wildcard $(SRC_DIR)/*.c))  $(patsubst %.c,%.o,$(wildcard $(MOD_DIR/*.c))
#COMPILE.c = $(CC) $(CFLAGS) -c
##COMPILE.c = $(CC) $(CFLAGS) -MMD -MF $(DEP_DIR)/$*.d -c
#
#all: $(DEP_DIR) jobCommander jobExecutorServer
#
#jobCommander: $(SRC_DIR)/jobCommander.o
#	$(CC) $(CFLAGS) $^ -o $@
#
#jobExecutorServer: $(SRC_DIR)/jobExecutorServer.o $(MODS)
#	$(CC) $(CFLAGS) $^ -o $@
#
#$(DEP_DIR):
#	mkdir -p $@
#
#clean:
#	rm jobExecutorServer jobCommander $(SRC_DIR)/*.o $(MOD_DIR)/*.o jobCommanderServer.txt
#
#-include $(wildcard $(DEPFILES))


CC = gcc
DEPFLAGS = -MT $@ -MMD -MP
CFLAGS= -g -Wall -Iinclude
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) -c
MODS := $(wildcard modules/*.c)
SRCS := $(wildcard source/*.c)
OBJS := $(SRCS:%.c=%.o) $(MODS:%.c=%.o)
DEPS := $(OBJS:%.o=%.d)
#all: jobCommander jobExecutorServer
#
#jobCommander: sources/jobCommander.o
#	$(CC) $(CFLAGS) $^ -o $@
#
#jobExecutorServer: sources/jobExecutorServer.c $(MODS:.c=.o)
#	$(CC) $(CFLAGS) $^ -o $@
#.PHONY: rebuild
#rebuild:
#		$(MAKE) clean
#		$(MAKE) all

all: jobCommander jobExecutorServer
jobCommander: source/jobCommander.o
		$(CC) $(CFLAGS) $^ -o $@

jobExecutorServer: source/jobExecutorServer.o $(MODS:%.c=%.o)
		$(CC) $(CFLAGS) $^ -o $@

%.o : %.c
		$(COMPILE.c) $(OUTPUT_OPTION) $<
pipes_clean:
	rm -rf /tmp/sdi1900096*
clean:
	rm -rf jobExecutorServer jobCommander $(OBJS) $(DEPS) my_A* my_B* *.txt

-include $(DEPS)