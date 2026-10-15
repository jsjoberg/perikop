# onnxruntime.dll needs the Visual C++ runtime, which a clean Windows may lack.
set(PERIKOP_MSVC_RUNTIME "" CACHE PATH "Microsoft.VC14x.CRT folder to install beside onnxruntime.dll")
if(PERIKOP_MSVC_RUNTIME)
    foreach(library msvcp140 msvcp140_1 vcruntime140 vcruntime140_1)
        install(FILES "${PERIKOP_MSVC_RUNTIME}/${library}.dll" DESTINATION bin)
    endforeach()
else()
    message(WARNING "PERIKOP_MSVC_RUNTIME is not set. Installed Perikop needs the Visual C++ runtime.")
endif()
