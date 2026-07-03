add_subdirectory(crush)
add_subdirectory(tune)

add_executable(mine mine.cpp)
target_link_libraries(mine PRIVATE config raw_reader ROOT::RIO ROOT::Tree)

add_executable(crush_vme crush_vme.cpp)
target_link_libraries(
	crush_vme
	PRIVATE
	config
	tafd_event
	dssd_event
	csi_event
	beam_event
	ppac_event
	ROOT::RIO
	ROOT::Tree
	ROOT::Core
)

add_executable(coke coke.cpp)
target_link_libraries(
	coke
	PRIVATE
	config
	raw_trigger_event trigger_event align_event
	ROOT::Core ROOT::RIO ROOT::Tree ROOT::Graf
)

add_executable(pre_coke_ppac pre_coke_ppac.cpp)
target_link_libraries(
	pre_coke_ppac
	PRIVATE
	config
	raw_trigger_event raw_ppac_event
	ROOT::Core ROOT::RIO ROOT::Tree ROOT::Graf
)

add_executable(smelt smelt.cpp)
target_link_libraries(
	smelt
	PRIVATE
	config
	smelt_ppac
	smelt_t0d1
	smelt_t0d2
	smelt_t0d3
	smelt_t0d4
	smelt_beam
	smelt_ssd
	smelt_t0s_trace
	smelt_t0csi_bloom # only have XIA's T0CsI
	smelt_t0csi       # have T0CsI in both vme and XIA
	smelt_t0csi_trace
	csi_event
	dssd_event
	tafd_event
	align_event
	trigger_event
	ppac_event
	ROOT::Core
	ROOT::Hist
	ROOT::RIO
	ROOT::Tree
)

add_executable(sift sift.cpp)
target_link_libraries(sift PRIVATE sifting trigger_event raw_trigger_event config)