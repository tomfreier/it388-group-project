APPNAME = grayscale
CC = mpicc
CFLAGS = -Wall -O3 -march=native -mtune=native -pipe -fopenmp
LDFLAGS = -flto -fuse-linker-plugin -pipe -fopenmp
LDLIBS = -lm
SRCDIR = .
OBJDIR = ./obj

# gets all .c files in the source directory
SRC = $(wildcard $(SRCDIR)/*.c)
# takes all filenames from SRC and replaces .c with .o
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# ensures object dir is created prior to all the compilation jazz
all: | create_obj_dir $(APPNAME)

# compiles object files into target executable APPNAME
$(APPNAME): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# compiles .o files into OBJDIR from .c files found in SRCDIR
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -o $@ -c $< 

#clean project for submission
clean:
	rm -rf $(OBJDIR) $(APPNAME)

#creates object dir if it does not exist
create_obj_dir:
ifeq ("$(wildcard $(OBJDIR))","") #thanks stack overflow
	mkdir $(OBJDIR)
endif
