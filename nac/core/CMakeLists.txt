file(GLOB tmp *.cc)
set(_nac_SRC ${_nac_SRC} ${tmp})

file(GLOB tmp brew/*.cc)
set(_nac_SRC ${_nac_SRC} ${tmp})
set(_nac_SRC ${_nac_SRC} PARENT_SCOPE)