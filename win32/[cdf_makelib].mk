
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES:.cpp=.o)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES:.c=.o)

LOCAL_OBJ_FILES := $(addprefix obj/$(xtarget)/, $(LOCAL_SRC_FILES))

ifneq ($(filter _x86BCC_ _x86_64BCC_,$(xtarget)),)
  LOCAL_OBJ_FILES_CORRECT=$(subst /,\,$(LOCAL_OBJ_FILES))
endif

all : $(LOCAL_MODULE)
 
$(LOCAL_MODULE) : obj_dirs $(LOCAL_OBJ_FILES)
  @echo $(xtarget)  $@.a ...
ifneq ($(filter [x86GCC] [x86_64GCC],$(xtarget)),)
    @ar rcs $(prextarget)/$(xtarget)/$@.a $(LOCAL_OBJ_FILES)
endif
ifneq ($(filter [x86VCC] [x86_64VCC],$(xtarget)),)
    @lib /nologo /OUT:$(prextarget)/$(xtarget)/$@.a $(LOCAL_OBJ_FILES)
endif
ifneq ($(filter [x86ICC] [x86_64ICC],$(xtarget)),)
    @xilib /nologo /OUT:$(prextarget)/$(xtarget)/$@.a $(LOCAL_OBJ_FILES)
endif
ifneq ($(filter [x86CLANG] [x86_64CLANG],$(xtarget)),)
    @llvm-ar rcs $(prextarget)/$(xtarget)/$@.a $(LOCAL_OBJ_FILES)
endif

obj_dirs :
  @DIRSCCT.exe $(xtarget) $(LOCAL_SRC_FILES)

obj/$(xtarget)/%.o : %.cpp
  @echo [C++] $(xtarget) $< ...
ifneq ($(filter [x86GCC] [x86_64GCC],$(xtarget)),)
    @g++ -o $@ -c $< $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86VCC] [x86_64VCC],$(xtarget)),)
    @cl /Fo$@ $< $(LOCAL_CFLAGS) $(addprefix /D, $(LOCAL_C_DEFINES)) $(addprefix /I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86ICC] [x86_64ICC],$(xtarget)),)
    @icl /Fo$@ $< $(LOCAL_CFLAGS) $(addprefix /D, $(LOCAL_C_DEFINES)) $(addprefix /I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86CLANG],$(xtarget)),)
    @clang++ --target=i686-w64-mingw32 $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES)) -c $< -o $@
endif
ifneq ($(filter [x86_64CLANG],$(xtarget)),)
    @clang++ --target=x86_64-w64-mingw32 $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES)) -c $< -o $@
endif
 
obj/$(xtarget)/%.o : %.c
  @echo [C] $(xtarget) $< ...
ifneq ($(filter [x86GCC] [x86_64GCC],$(xtarget)),)
    @gcc -o $@ -c $< $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86VCC] [x86_64VCC],$(xtarget)),)
    @cl /Fo$@ $< $(LOCAL_CFLAGS) $(addprefix /D, $(LOCAL_C_DEFINES)) $(addprefix /I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86ICC] [x86_64ICC],$(xtarget)),)
    @icl /Fo$@ $< $(LOCAL_CFLAGS) $(addprefix /D, $(LOCAL_C_DEFINES)) $(addprefix /I, $(LOCAL_C_INCLUDES))
endif
ifneq ($(filter [x86CLANG],$(xtarget)),)
    @clang --target=i686-w64-mingw32 $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES)) -c $< -o $@
endif
ifneq ($(filter [x86_64CLANG],$(xtarget)),)
    @clang --target=x86_64-w64-mingw32 $(LOCAL_CFLAGS) $(addprefix -D, $(LOCAL_C_DEFINES)) $(addprefix -I, $(LOCAL_C_INCLUDES)) -c $< -o $@
endif

.PHONY : clean
 
clean :
  rm -rf bin obj
 
include $(wildcard $(addsuffix /*.d, $(objects_dirs)))
