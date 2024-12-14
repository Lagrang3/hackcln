EXAMPLE_SRC := $(wildcard example/ex-*.c)
EXAMPLE_OBJS := $(EXAMPLE_SRC:.c=.o)
EXAMPLE_PROGRAMS := $(EXAMPLE_OBJS:.o=)

ALL_C_SOURCES += $(EXAMPLE_SRC)
ALL_PROGRAMS += $(EXAMPLE_PROGRAMS)

$(EXAMPLE_OBJS): $(EXAMPLE_SRC)

example/ex-pipe: example/ex-pipe.o

example/ex-pipe-app: example/ex-pipe-app.o
