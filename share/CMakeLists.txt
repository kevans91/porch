option(EXAMPLES "Install examples" ON)
option(MANPAGES "Install manpages" ON)

if(EXAMPLES)
	add_subdirectory(examples)
endif()
if(MANPAGES)
	add_subdirectory(man)
endif()
