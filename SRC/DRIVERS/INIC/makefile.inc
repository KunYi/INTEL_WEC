!IF 0

Used to copy DLL and REL files of precompiled drivers to the
target (and optionally release) directory.

!ENDIF

BinaryCopy:
	copy /y Drivers\*.dll $(SG_OUTPUT_ROOT)\platform\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)
	copy /y Drivers\*.rel $(SG_OUTPUT_ROOT)\platform\$(_TGTPLAT)\target\$(_TGTCPU)\$(WINCEDEBUG)
!IF "$(WINCEREL)"=="1"
	copy /y Drivers\*.dll $(_FLATRELEASEDIR)
	copy /y Drivers\*.rel $(_FLATRELEASEDIR)
!ENDIF
