
set(MANSECTIONS 1 3lua 5)

set(MAN1	porch.1 rporch.1)
set(MAN3lua	porch.3lua)
set(MAN5	orch.5)

foreach(sect IN LISTS MANSECTIONS)
	foreach(page IN LISTS MAN${sect})
		install(FILES man${sect}/${page} DESTINATION "share/man/man${sect}")
	endforeach()
endforeach()

add_custom_target(check-man
	COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/lint-man.sh"
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
