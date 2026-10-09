# 主程序和独立诊断程序共用仓库内的原生 SimConnect DLL。
function(msfs_deploy_runtime target)
    set(runtime_dll "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SimConnect.dll")
    if(WIN32 AND EXISTS "${runtime_dll}")
        # 每次构建都执行 copy_if_different；替换 DLL 后不必触发重新链接。
        add_custom_target(${target}_simconnect_runtime
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${runtime_dll}" "$<TARGET_FILE_DIR:${target}>/SimConnect.dll"
                DEPENDS "${runtime_dll}"
                VERBATIM)
        add_dependencies(${target} ${target}_simconnect_runtime)
    endif()
endfunction()
