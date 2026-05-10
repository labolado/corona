# Minimal replacement for deprecated AndroidNdkModules.cmake (removed in NDK r24+)
# Provides android_ndk_import_module_cpufeatures() for legacy compatibility

function(android_ndk_import_module_cpufeatures)
    if(TARGET cpufeatures)
        return()
    endif()
    # Find NDK root
    set(NDK_ROOT "")
    if(CMAKE_ANDROID_NDK)
        set(NDK_ROOT "${CMAKE_ANDROID_NDK}")
    elseif(ANDROID_NDK)
        set(NDK_ROOT "${ANDROID_NDK}")
    elseif(ENV{ANDROID_NDK})
        set(NDK_ROOT "$ENV{ANDROID_NDK}")
    elseif(ENV{ANDROID_NDK_HOME})
        set(NDK_ROOT "$ENV{ANDROID_NDK_HOME}")
    endif()

    set(CPUFEATURES_C "${NDK_ROOT}/sources/android/cpufeatures/cpu-features.c")
    set(CPUFEATURES_H "${NDK_ROOT}/sources/android/cpufeatures/cpu-features.h")

    if(EXISTS "${CPUFEATURES_C}" AND EXISTS "${CPUFEATURES_H}")
        add_library(cpufeatures STATIC "${CPUFEATURES_C}")
        target_include_directories(cpufeatures PUBLIC "${NDK_ROOT}/sources/android/cpufeatures")
    else()
        # Fallback: create interface library if sources not found
        add_library(cpufeatures INTERFACE)
        message(WARNING "cpufeatures sources not found in NDK. Creating dummy target.")
    endif()
endfunction()
