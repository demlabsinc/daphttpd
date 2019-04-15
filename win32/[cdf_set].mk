
ifneq ($(filter [x86GCC],$(xtarget)),)
	LOCAL_CFLAGS := -Wall -O3 -fno-ident -fno-exceptions -ffast-math -ftree-vectorize -mfpmath=sse -mmmx -msse2 -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
	LOCAL_LFLAGS := -mwindows -static -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -fno-ident -Wl,--strip-all

	ifneq ($(filter TRUE,$(CPP_11_FLAG)),)
		LOCAL_CFLAGS += -std=c++11
	endif

	ifneq ($(filter TRUE,$(C_11_FLAG)),)
		LOCAL_CFLAGS += -std=c11
	endif

	LOCAL_C_INCLUDES += E:/LLVM/i686-w64-mingw32/include/C++ \
						E:/LLVM/i686-w64-mingw32/include/C++/i686-w64-mingw32
endif

ifneq ($(filter [x86_64GCC],$(xtarget)),)
	LOCAL_CFLAGS := -Wall -O3 -fno-ident -fno-exceptions -ffast-math -ftree-vectorize -mfpmath=sse -mmmx -msse2 -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -Wl,--strip-all
	LOCAL_LFLAGS := -mwindows -static -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -fno-ident -Wl,--strip-all

	ifneq ($(filter TRUE,$(CPP_11_FLAG)),)
		LOCAL_CFLAGS += -std=c++11
	endif

	ifneq ($(filter TRUE,$(C_11_FLAG)),)
		LOCAL_CFLAGS += -std=c11
	endif

	LOCAL_C_INCLUDES += E:/LLVM/x86_64-w64-mingw32/include/C++ \
						E:/LLVM/x86_64-w64-mingw32/include/C++/x86_64-w64-mingw32
endif

ifneq ($(filter [x86VCC] [x86_64VCC],$(xtarget)),)
	LOCAL_CFLAGS := /nologo /GR- /GS- /O2 /Qvec-report:1 /c /MT /W3 /fp:fast /EHsc

	LOCAL_C_INCLUDES += D:/home/vcc/include \
						D:/home/vcc/8.1/Include/shared \
						D:/home/vcc/8.1/Include/um \
						D:/home/vcc/8.1/Include/winrt \
						D:/home/vcc/10/Include/10.0.10240.0/ucrt

	LOCAL_LFLAGS := /SUBSYSTEM:WINDOWS /LIBPATH:lib/$(xtarget)
endif

ifneq ($(filter [x86ICC],$(xtarget)),)

	LOCAL_CFLAGS := /c /Qvc12 /Qlocation,link,C:\Bin64\prg\cpp\vcc12\bin /FI ../intelshit.h \
					/nologo /W3 /O2 /D__INTEL_COMPILER=1400 /MT /GS

	LOCAL_C_INCLUDES += E:/bin/icc/include \
						D:/Bin64/prg/cpp/vcc12/include \
						D:/Bin64/prg/cpp/vcc12/8.1/Include/shared \
						D:/Bin64/prg/cpp/vcc12/8.1/Include/um \
						D:/Bin64/prg/cpp/vcc12/8.1/Include/winrt

	LOCAL_LFLAGS := /SUBSYSTEM:WINDOWS /LIBPATH:../../src/lib/$(xtarget)

endif

ifneq ($(filter [x86_64ICC],$(xtarget)),)

	LOCAL_CFLAGS := /c /Qvc12 /Qlocation,link,C:\Bin64\prg\cpp\vcc12\bin\amd64 /FI ../intelshit.h \
					/nologo /W3 /O2 /D__INTEL_COMPILER=1400 /MT /GS

	LOCAL_C_INCLUDES += E:/bin/icc/include \
						C:/Bin64/prg/cpp/vcc12/include \
						C:/Bin64/prg/cpp/vcc12/8.1/Include/shared \
						C:/Bin64/prg/cpp/vcc12/8.1/Include/um \
						C:/Bin64/prg/cpp/vcc12/8.1/Include/winrt

	LOCAL_LFLAGS := /SUBSYSTEM:WINDOWS /LIBPATH:../../src/lib/$(xtarget)
endif

ifneq ($(filter [x86_64VCC],$(xtarget)),)
	LOCAL_LFLAGS += /MACHINE:X64 \
					/LIBPATH:D:/home/vcc/lib/amd64 \
					/LIBPATH:D:/home/vcc/8.1/Lib/winv6.3/um/x64 \
					/LIBPATH:D:/home/vcc/10/Lib/10.0.10240.0/ucrt/x64
endif
ifneq ($(filter [x86VCC],$(xtarget)),)
	LOCAL_LFLAGS += /MACHINE:IX86 \
					/LIBPATH:D:/home/vcc/lib \
					/LIBPATH:D:/home/vcc/8.1/Lib/winv6.3/um/x86 \
					/LIBPATH:D:/home/vcc/10/Lib/10.0.10240.0/ucrt/x86
endif

ifneq ($(filter [x86_64ICC],$(xtarget)),)
	LOCAL_LFLAGS += /MACHINE:X64 /LIBPATH:c:/bin64/prg/cpp/vcc12/lib/amd64 \
					/LIBPATH:C:/Bin64/prg/cpp/vcc12/8.1/Lib/winv6.3/um/x64 \
					/LIBPATH:D:/bin/icc/lib/intel64

endif
ifneq ($(filter [x86ICC],$(xtarget)),)
	LOCAL_LFLAGS += /MACHINE:IX86 /LIBPATH:c:/bin64/prg/cpp/vcc12/lib \
					/LIBPATH:C:/Bin64/prg/cpp/vcc12/8.1/Lib/winv6.3/um/x86 \
					/LIBPATH:D:/bin/icc/lib/ia32
endif

ifneq ($(filter [x86CLANG],$(xtarget)),)

	LOCAL_CFLAGS := -Wall -mmmx -msse2 -O3 -Rpass=loop-vectorize -fno-exceptions -fvisibility=hidden -ffast-math -ftree-vectorize -mfpmath=sse -fno-asynchronous-unwind-tables -ffunction-sections 
	LOCAL_LFLAGS := -static -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -fno-ident -fno-asynchronous-unwind-tables -Wl,--strip-all

	ifneq ($(filter TRUE,$(CPP_11_FLAG)),)
		LOCAL_CFLAGS += -std=c++11
	endif

	ifneq ($(filter TRUE,$(C_11_FLAG)),)
		LOCAL_CFLAGS += -std=c11
	endif

	LOCAL_C_INCLUDES += D:/LLVM/i686-w64-mingw32/include/C++ \
						D:/LLVM/i686-w64-mingw32/include/C++/i686-w64-mingw32 \

endif

ifneq ($(filter [x86_64CLANG],$(xtarget)),)

	LOCAL_CFLAGS := -Wall -mmmx -msse2 -O3 -Rpass=loop-vectorize -fno-exceptions -fvisibility=hidden -ffast-math -ftree-vectorize -mfpmath=sse -fno-asynchronous-unwind-tables -ffunction-sections 
	LOCAL_LFLAGS := -static -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -fno-ident -fno-asynchronous-unwind-tables -Wl,--strip-all

	ifneq ($(filter TRUE,$(CPP_11_FLAG)),)
		LOCAL_CFLAGS += -std=c++11
	endif

	ifneq ($(filter TRUE,$(C_11_FLAG)),)
		LOCAL_CFLAGS += -std=c11
	endif

	LOCAL_C_INCLUDES += D:/LLVM/x86_64-w64-mingw32/include/C++ \
						D:/LLVM/x86_64-w64-mingw32/include/C++/x86_64-w64-mingw32 \

endif

ifneq ($(filter [x86ICC] [x86VCC],$(xtarget)),)

	LOCAL_CFLAGS += /arch:SSE2

endif

