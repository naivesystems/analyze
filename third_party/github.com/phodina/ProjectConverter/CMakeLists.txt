# CMake minimum version
cmake_minimum_required (VERSION {{ cmake.version }})

# Project Infomation
project( {{ cmake.project }} )
enable_language(ASM)
enable_language(C)

# Reset output path
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set (LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# STDLIB
set (CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)

# Set include path
{% for inc in cmake.incs %}
include_directories ({{ inc }})
{% endfor %}

# The need build source path and build all files
{% for src in cmake.files %}
set ({{ src.var }} {{ src.path }})
{% endfor %}

{% for as in cmake.ass %}
set_source_files_properties({{ as.path }} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
{% endfor %}

set(CROSS_TARGET_TRIPLET "arm-none-eabi-")

# CC AR LD AS
set(CMAKE_C_COMPILER "${CROSS_TARGET_TRIPLET}gcc")
{%if cmake.cxx == 'true' %}
set(CMAKE_CXX_COMPILER "${CROSS_TARGET_TRIPLET}g++")
{% endif %}
set(CMAKE_ASM_COMPILER "${CROSS_TARGET_TRIPLET}gcc")

# CFLAGS
set (CMAKE_C_FLAGS "{{ cmake.c_flags }}" CACHE INTERNAL "c compiler flags")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} {% for define in cmake.defines %} -D {{ define }} {% endfor %}")

# CXXFLAGS
set (CMAKE_CXX_FLAGS "{{ cmake.cxx_flags }}" CACHE INTERNAL "cxx compiler flags")

# ASMFLAGS
set (CMAKE_ASM_FLAGS "{{ cmake.asm_flags }}" CACHE INTERNAL "asm compiler flags")

# LDFLAGS
set (CMAKE_EXE_LINKER_FLAGS "{{ cmake.linker_flags }}" CACHE INTERNAL "executable linker flags")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_SOURCE_DIR}/{{ cmake.linker_script }} -L {{ cmake.linker_path }}")

{% for lib in cmake.libs %}
# Load the the extern library
set ({{ lib.name }} ${CMAKE_SOURCE_DIR}/{{ lib.path }}/{{ lib.name }}.a)
#add_library ({{ lib.name }} STATIC IMPORTED)
#set_property ({{ lib.name }} PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/{{ lib.path }}/{{ lib.name }}.a)
{% endfor %}

# Generate the target
add_executable (${CMAKE_PROJECT_NAME}.elf {% for src in cmake.files %} {{ '${' }}{{ src.var }}{{ '}' }} {% endfor %})

# Link the library to the target
target_link_libraries (${CMAKE_PROJECT_NAME}.elf {% for lib in cmake.libs %} {{ lib.name }} {% endfor %})

# Generate the binary file
add_custom_target (${CMAKE_PROJECT_NAME}.bin ALL arm-none-eabi-objcopy -Obinary "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf" "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.bin" DEPENDS ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf)

# Generate the hex file
add_custom_target (${CMAKE_PROJECT_NAME}.hex ALL arm-none-eabi-objcopy -Oihex "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf" "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.hex" DEPENDS ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf)

# Echo the size Infomation
add_custom_target (size ALL arm-none-eabi-size "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf" DEPENDS ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.elf)

# Make flash to the board by st-link
add_custom_target (flash COMMAND st-flash write ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_PROJECT_NAME}.bin 0x8000000)

# Make clean-all
add_custom_target (clean-all COMMAND rm -rf ${CMAKE_BINARY_DIR}/*)
