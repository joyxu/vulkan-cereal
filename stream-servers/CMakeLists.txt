# Vulkan
add_subdirectory(vulkan)

# GLES
add_subdirectory(glestranslator)
add_subdirectory(libGLSnapshot)
add_subdirectory(gles1_dec)
add_subdirectory(gles2_dec)

# GLES dispatch based on Translator
add_subdirectory(OpenGLESDispatch)

# RenderControl
add_subdirectory(renderControl_dec)
