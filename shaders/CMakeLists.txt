find_program(GLSLANG "glslangValidator")

macro(add_shader inName outName)
    if (GLSLANG)
        set(SHADER_IN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${inName}.glsl")
        set(SHADER_OUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shader_${outName}.h")
        list(APPEND SHADER_OUT_NAMES ${SHADER_OUT_PATH})
        add_custom_command(
            MAIN_DEPENDENCY ${SHADER_IN_PATH}
            OUTPUT ${SHADER_OUT_PATH}
            COMMAND ${GLSLANG} ${SHADER_IN_PATH} -o ${SHADER_OUT_PATH} --target-env vulkan1.3 --vn "shader_${outName}" ${ARGN}
            VERBATIM)
    endif()
endmacro()

add_shader("transfer.comp" "transfer")
add_shader("draw.comp" "draw")

add_custom_target(woof-build-shaders DEPENDS ${SHADER_OUT_NAMES})
add_library(woof-shaders INTERFACE)
add_dependencies(woof-shaders woof-build-shaders)
target_include_directories(woof-shaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})