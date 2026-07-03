add_executable(crush_ppac crush_ppac.cpp)
target_link_libraries(crush_ppac PRIVATE config crusher raw_ppac_event)

add_executable(crush_trigger crush_trigger.cpp)
target_link_libraries(crush_trigger PRIVATE config raw_reader raw_trigger_event)

add_executable(crush_beam crush_beam.cpp)
target_link_libraries(crush_beam PRIVATE config crusher raw_beam_event)

add_executable(crush_t0d1 crush_t0d1.cpp)
target_link_libraries(crush_t0d1 PRIVATE config crusher raw_dssd_event)

add_executable(crush_t0d2 crush_t0d2.cpp)
target_link_libraries(crush_t0d2 PRIVATE config crusher raw_dssd_event)

add_executable(crush_t0d3 crush_t0d3.cpp)
target_link_libraries(crush_t0d3 PRIVATE config crusher raw_dssd_event)

add_executable(crush_t0d4 crush_t0d4.cpp)
target_link_libraries(crush_t0d4 PRIVATE config crusher raw_dssd_event)

add_executable(crush_others crush_others.cpp)
target_link_libraries(crush_others PRIVATE config crusher raw_csi_event raw_ssd_event)

add_executable(crush_t0csi_trace crush_t0csi_trace.cpp)
target_link_libraries(crush_t0csi_trace PRIVATE config crusher raw_csi_event)

add_executable(crush_t0s_trace crush_t0s_trace.cpp)
target_link_libraries(crush_t0s_trace PRIVATE config crusher raw_ssd_event)