ECHO Copying server binaries
	CALL copy_server.bat %1
ECHO DONE
ECHO

ECHO Copying SDK binaries
	CALL copy_sdk.bat ..\%1\
ECHO DONE
ECHO.

ECHO Copying documentation
	CALL documentation_deployment\compileAllDocumentation.bat ..\%1\
ECHO DONE
ECHO.

ECHO Building GUI
	CALL gui_autobuild\buildgui.bat ..\%1\gui
ECHO DONE
ECHO.

ECHO Building module creator
	CALL module_creator_autobuild\buildmodulecreator.bat ..\%1\sdk\ModuleCreator
ECHO DONE
ECHO.

ECHO Copying modules
	CALL copy_modules.bat ..\%1\
ECHO DONE
ECHO.

ECHO Copying GUI block library
	CALL copy_block_library.bat ..\%1\gui\BlockLibrary
ECHO DONE
ECHO.

if %1 eq Debug
(
ECHO Copying GUI debug block library
	CALL copy_block_library.bat ..\%1\gui-debug\BlockLibrary
ECHO DONE
ECHO.
)