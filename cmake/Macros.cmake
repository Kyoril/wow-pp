MACRO(ADD_PRECOMPILED_HEADER _targetName _input)
	GET_FILENAME_COMPONENT(_name ${_input} NAME)
	get_filename_component(_name_no_ext ${_name} NAME_WE)
	if(MSVC)
		set_target_properties(${_targetName} PROPERTIES COMPILE_FLAGS "/Yu${_name}")
		set_source_files_properties("${_name_no_ext}.cpp" PROPERTIES COMPILE_FLAGS "/Yc${_name}")
	else()
		IF(APPLE)
			set_target_properties(
        		    ${_targetName} 
        		    PROPERTIES
        		    XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/${_name_no_ext}.h"
        		    XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
        		)
		ELSE()
			if(UNIX)
				GET_FILENAME_COMPONENT(_path ${_input} PATH)
				SET(_outdir "${CMAKE_CURRENT_BINARY_DIR}")
				SET(_output "${_input}.gch")

				set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES _output)

				STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
				SET(_compile_FLAGS ${${_flags_var_name}})

				GET_DIRECTORY_PROPERTY(_directory_flags INCLUDE_DIRECTORIES)
				foreach(d ${_directory_flags})
                                        #-isystem to ignore warnings in foreign headers
					list(APPEND _compile_FLAGS "-isystem${d}")
				endforeach()

				GET_DIRECTORY_PROPERTY(_directory_flags COMPILE_DEFINITIONS)
				LIST(APPEND defines ${_directory_flags})

				string(TOUPPER "COMPILE_DEFINITIONS_${CMAKE_BUILD_TYPE}" defines_for_build_name)
				get_directory_property(defines_build ${defines_for_build_name})
				list(APPEND defines ${defines_build})

				foreach(item ${defines})
					list(APPEND _compile_FLAGS "-D${item}")
				endforeach()

				LIST(APPEND _compile_FLAGS ${CMAKE_CXX_FLAGS})

				SEPARATE_ARGUMENTS(_compile_FLAGS)
				ADD_CUSTOM_COMMAND(
					OUTPUT ${_output}
					COMMAND ${CMAKE_CXX_COMPILER}
					${_compile_FLAGS}
					-x c++-header
					-fPIC
					-o ${_output}
					${_input}
					DEPENDS ${_input} #${CMAKE_CURRENT_BINARY_DIR}/${_name}
				)
				ADD_CUSTOM_TARGET(${_targetName}_gch
					DEPENDS	${_output}
				)
				ADD_DEPENDENCIES(${_targetName} ${_targetName}_gch)
			endif()
		ENDIF()
	endif()
ENDMACRO()

MACRO(remove_pch_cpp sources pch_cpp)
	if(UNIX)
		list(REMOVE_ITEM ${sources} ${pch_cpp})
	endif()
ENDMACRO()
