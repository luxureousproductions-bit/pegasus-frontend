# Pegasus X: audio decoders for AudioSpectrum
add_library(audiodec STATIC audiodec.cpp)
target_include_directories(audiodec PUBLIC .)
set_target_properties(audiodec PROPERTIES POSITION_INDEPENDENT_CODE ON)
