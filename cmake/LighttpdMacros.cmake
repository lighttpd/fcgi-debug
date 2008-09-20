
MACRO(ADD_TARGET_PROPERTIES _target _name)
  SET(_properties)
  FOREACH(_prop ${ARGN})
    SET(_properties "${_properties} ${_prop}")
  ENDFOREACH(_prop)
  GET_TARGET_PROPERTY(_old_properties ${_target} ${_name})
	MESSAGE("adding property to ${_target} ${_name}:" ${_properties})
  IF(NOT _old_properties)
    # in case it's NOTFOUND
    SET(_old_properties)
  ENDIF(NOT _old_properties)
  SET_TARGET_PROPERTIES(${_target} PROPERTIES ${_name} "${_old_properties} ${_properties}")
ENDMACRO(ADD_TARGET_PROPERTIES)
