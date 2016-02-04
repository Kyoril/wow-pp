
if (MSVC)
	# ignore annoying protobuf-map-related warning
	add_definitions("/wd4800")
endif()

file(GLOB srcFiles "*.cpp" "*.h")
file(GLOB protoFiles "*.proto")
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${protoFiles})

add_library(proto_data ${PROTO_SRCS} ${PROTO_HDRS} ${srcFiles})

# Solution folder
if(MSVC)
	set_property(TARGET proto_data PROPERTY FOLDER "shared")
endif(MSVC)