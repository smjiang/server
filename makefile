GCC = g++
CFLAGS = -fPIC -pthread -D_FILE_OFFSET_BITS=64 -Wall -o2 -fpermissive

OBJ_PATH = objs/
EXE_PATH = bin/
EXE_NAME = fileserver
ALL_OBJ = $(addprefix $(OBJ_PATH),$(addsuffix .o,$(basename $(ALL_CPP))))
ALL_CPP = $(wildcard *.cpp)


$(EXE_NAME) : $(ALL_OBJ)
	@-mkdir -p $(EXE_PATH)
	$(GCC) $^ $(CFLAGS) -o $(EXE_PATH)$(EXE_NAME)

$(ALL_OBJ) : $(OBJ_PATH)%.o : %.cpp
	@mkdir -p $(OBJ_PATH)$(dir $<)
	$(GCC) $(CFLAGS) -c $< -o $@

.PHONY : clean
clean :
	rm -f $(EXE_PATH)$(EXE_NAME).so $(ALL_OBJ)