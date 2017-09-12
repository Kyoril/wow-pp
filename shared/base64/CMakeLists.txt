#include_directories(".")

file(GLOB codeFiles
	"*.h"
	"*.cpp"
)

add_library(base64 STATIC ${codeFiles})

# Solution folder
if(MSVC)
	set_property(TARGET base64 PROPERTY FOLDER "shared")
endif(MSVC)
