
FIND_PACKAGE(MFC)

if (NOT MFC_FOUND)
	message(STATUS "MFC could not be found, not building FRED")
	return()
endif()

set(FRED2_SOURCES
	altshipclassdlg.cpp
	altshipclassdlg.h
	fictionviewerdlg.cpp
	fictionviewerdlg.h
	addvariabledlg.cpp
	addvariabledlg.h
	adjustgriddlg.cpp
	adjustgriddlg.h
	asteroideditordlg.cpp
	asteroideditordlg.h
	backgroundchooser.cpp
	backgroundchooser.h
	bgbitmapdlg.cpp
	bgbitmapdlg.h
	briefingeditordlg.cpp
	briefingeditordlg.h
	campaigneditordlg.cpp
	campaigneditordlg.h
	campaignfilelistbox.cpp
	campaignfilelistbox.h
	campaigntreeview.cpp
	campaigntreeview.h
	campaigntreewnd.cpp
	campaigntreewnd.h
	cmdbrief.cpp
	cmdbrief.h
	createwingdlg.cpp
	createwingdlg.h
	customwingnames.cpp
	customwingnames.h
	debriefingeditordlg.cpp
	debriefingeditordlg.h
	dialog1.cpp
	dialog1.h
	dumpstats.cpp
	dumpstats.h
	editor.h
	eventeditor.cpp
	eventeditor.h
	folderdlg.cpp
	folderdlg.h
	fred.cpp
	fred.h
	fred.rc
	freddoc.cpp
	freddoc.h
	fredrender.cpp
	fredrender.h
	fredstubs.cpp
	fredview.cpp
	fredview.h
	grid.cpp
	grid.h
	ignoreordersdlg.cpp
	ignoreordersdlg.h
	initialships.cpp
	initialships.h
	initialstatus.cpp
	initialstatus.h
	mainfrm.cpp
	mainfrm.h
	management.cpp
	management.h
	messageeditordlg.cpp
	messageeditordlg.h
	missiongoalsdlg.cpp
	missiongoalsdlg.h
	missionnotesdlg.cpp
	missionnotesdlg.h
	missionsave.cpp
	missionsave.h
	modifyvariabledlg.cpp
	modifyvariabledlg.h
	operatorargtypeselect.cpp
	operatorargtypeselect.h
	orienteditor.cpp
	orienteditor.h
	playerstarteditor.cpp
	playerstarteditor.h
	prefsdlg.cpp
	prefsdlg.h
	reinforcementeditordlg.cpp
	reinforcementeditordlg.h
	resource.h
	restrictpaths.cpp
	restrictpaths.h
	setglobalshipflags.cpp
	setglobalshipflags.h
	sexp_tree.cpp
	sexp_tree.h
	shieldsysdlg.cpp
	shieldsysdlg.h
	ship_select.cpp
	ship_select.h
	shipchecklistbox.cpp
	shipchecklistbox.h
	shipclasseditordlg.cpp
	shipclasseditordlg.h
	shipeditordlg.cpp
	shipeditordlg.h
	shipflagsdlg.cpp
	shipflagsdlg.h
	shipgoalsdlg.cpp
	shipgoalsdlg.h
	shipspecialdamage.cpp
	shipspecialdamage.h
	shipspecialhitpoints.cpp
	shipspecialhitpoints.h
	shiptexturesdlg.cpp
	shiptexturesdlg.h
	soundenvironmentdlg.cpp
	soundenvironmentdlg.h
	starfieldeditor.cpp
	starfieldeditor.h
	stdafx.cpp
	stdafx.h
	textviewdlg.cpp
	textviewdlg.h
	voiceactingmanager.cpp
	voiceactingmanager.h
	waypointpathdlg.cpp
	waypointpathdlg.h
	weaponeditordlg.cpp
	weaponeditordlg.h
	wing.cpp
	wing.h
	wing_editor.cpp
	wing_editor.h
)

set_source_files_properties(fred.rc PROPERTIES
	LANGUAGE RC
)

IF(MSVC_USE_RUNTIME_DLL)
	set(CMAKE_MFC_FLAG 2)
ELSE(MSVC_USE_RUNTIME_DLL)
	set(CMAKE_MFC_FLAG 1)
ENDIF(MSVC_USE_RUNTIME_DLL)

IF(MSVC60)
	link_directories(${STLPORT_INCLUDE_LIB_DIRS})
ENDIF(MSVC60)

add_executable(FRED2 WIN32 ${FRED2_SOURCES})

SET_TARGET_PROPERTIES(FRED2 PROPERTIES OUTPUT_NAME "fred2_open_${FSO_BINARY_SUFFIX}")

add_definitions(
	-DFRED
	-DWINVER=0x0501
	-D_WIN32_WINNT=0x0501
	-D_WINDOWS
	-DWIN32
	-D_CRT_SECURE_NO_DEPRECATE
	-D_SECURE_SCL=0
)

if (MSVC)
	set_target_properties(FRED2 PROPERTIES LINK_FLAGS "/FORCE:MULTIPLE")
endif()

INCLUDE_DIRECTORIES(${FREESPACE_HEADERS})
INCLUDE_DIRECTORIES(${GENERATED_SOURCE_DIR})

IF(USE_STLPORT)
	INCLUDE_DIRECTORIES(BEFORE SYSTEM ${STLPORT_INCLUDE_DIRS})
ENDIF(USE_STLPORT)

TARGET_LINK_LIBRARIES(FRED2 code) # This will also link all dependencies of code
TARGET_LINK_LIBRARIES(FRED2 platform)
TARGET_LINK_LIBRARIES(FRED2 compiler)
TARGET_LINK_LIBRARIES(FRED2 debug "nafxcwd.lib")
TARGET_LINK_LIBRARIES(FRED2 optimized "nafxcw.lib")

INSTALL(
	TARGETS FRED2
	RUNTIME DESTINATION ${BINARY_DESTINATION}
)
if (FSO_INSTALL_DEBUG_FILES)
	if (MSVC)
		install(FILES "$<TARGET_PDB_FILE:FRED2>"
				DESTINATION ${BINARY_DESTINATION}
				OPTIONAL)
	endif()
endif()

INCLUDE(util)
COPY_FILES_TO_TARGET(FRED2)

include(CreateLaunchers)
create_target_launcher(FRED2
	WORKING_DIRECTORY ${FSO_FREESPACE_PATH}
	ARGS ${FSO_RUN_ARGUMENTS})
