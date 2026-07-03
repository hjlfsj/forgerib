add_library(smelt_ppac smelt_ppac.cpp)
target_link_libraries(smelt_ppac PUBLIC ppac_event raw_ppac_event)

add_library(smelt_t0d1 smelt_t0d1.cpp)
target_link_libraries(smelt_t0d1 PUBLIC dssd_event raw_dssd_event)

add_library(smelt_t0d2 smelt_t0d2.cpp)
target_link_libraries(smelt_t0d2 PUBLIC dssd_event raw_dssd_event)

add_library(smelt_beam smelt_beam.cpp)
target_link_libraries(smelt_beam PUBLIC beam_event raw_beam_event)

add_library(smelt_ssd smelt_ssd.cpp)
target_link_libraries(smelt_ssd PUBLIC silicon_event raw_ssd_event)

add_library(smelt_t0s_trace smelt_t0s_trace.cpp)
target_link_libraries(smelt_t0s_trace PUBLIC silicon_event raw_ssd_event)

add_library(smelt_t0csi_bloom smelt_t0csi_bloom.cpp)
target_link_libraries(smelt_t0csi_bloom PUBLIC csi_event raw_csi_event)

add_library(smelt_t0csi smelt_t0csi.cpp)
target_link_libraries(smelt_t0csi PUBLIC csi_event align_event)

add_library(smelt_t0csi_trace smelt_t0csi_trace.cpp)
target_link_libraries(smelt_t0csi_trace PUBLIC csi_event align_event)

add_library(smelt_t0d3 smelt_t0d3.cpp)
target_link_libraries(smelt_t0d3 PUBLIC dssd_event raw_dssd_event)

add_library(smelt_t0d4 smelt_t0d4.cpp)
target_link_libraries(smelt_t0d4 PUBLIC dssd_event raw_dssd_event)

add_library(sifting sifting.cpp)
target_link_libraries(sifting PUBLIC align_event ROOT::Graf)