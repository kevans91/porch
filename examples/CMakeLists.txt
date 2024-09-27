
set(EXAMPLES
	cat.orch
	cat-multi.orch
	nc.orch
)

install(FILES ${EXAMPLES} DESTINATION
	${PORCHLUA_EXAMPLESDIR})
