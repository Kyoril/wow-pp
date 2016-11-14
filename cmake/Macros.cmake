function(enable_unity_build UB_SUFFIX SOURCE_VARIABLE_NAME)
	#(copied from http://cheind.wordpress.com/2009/12/10/reducing-compilation-time-unity-builds/ )
	set(files ${${SOURCE_VARIABLE_NAME}})
	list(LENGTH files fileCount)
	if(fileCount GREATER 2)
		# Generate a unique filename for the unity build translation unit
		set(unit_build_file ${CMAKE_CURRENT_BINARY_DIR}/ub_${UB_SUFFIX}.cpp)
		# Exclude all translation units from compilation
		set_source_files_properties(${files} PROPERTIES HEADER_FILE_ONLY true)
		# Open the ub file
		file(WRITE ${unit_build_file} "// Unity Build generated by CMake (do not edit this file)\n#include \"pch.h\"\n")
		# Add include statement for each translation unit
		foreach(source_file ${files})
			#adding pch.cpp is unnecessary
			if(NOT(${source_file} MATCHES ".*pch\\.cpp"))
				file(APPEND ${unit_build_file} "#include <${source_file}>\n")
			else()
				set_source_files_properties(${source_file} PROPERTIES HEADER_FILE_ONLY false)
			endif()
		endforeach()
		# Complement list of translation units with the name of ub
		set(${SOURCE_VARIABLE_NAME} ${${SOURCE_VARIABLE_NAME}} ${unit_build_file} PARENT_SCOPE)
	endif()
endfunction()

macro(add_lib name)
	file(GLOB sources "*.cpp")
	file(GLOB headers "*.h" "*.hpp")
	remove_pch_cpp(sources "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")
	if(WOWPP_UNITY_BUILD)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR})
		enable_unity_build(${name} sources)
	endif()
	add_library(${name} ${headers} ${sources})
	add_precompiled_header(${name} "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
	source_group(src FILES ${headers} ${sources})
endmacro()

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
