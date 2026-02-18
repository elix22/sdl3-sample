cmake_minimum_required(VERSION 3.16)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

# Declare the project
project(sdl-min)

# Set C++ standard globally (required by Abseil and MediaPipe)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if ((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Create an executable or a shared library based on the platform and add our sources to it
if (ANDROID)
	# The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
	set(EXECUTABLE_NAME main)
	add_library(${EXECUTABLE_NAME} SHARED)
else()
	add_executable(${EXECUTABLE_NAME})
endif()

# Add your sources to the target
target_sources(${EXECUTABLE_NAME} 
PRIVATE 
    src/testcamera.cpp
    src/iosLaunchScreen.storyboard
)
# What is iosLaunchScreen.storyboard? This file describes what Apple's mobile platforms
# should show the user while the application is starting up. If you don't include one,
# then you get placed in a compatibility mode that does not allow HighDPI.
# This file is referenced inside Info.plist.in, where it is marked as the launch screen file.
# It is also ignored on non-Apple platforms. 

# To get an app icon on Apple platforms, add it to your executable.
# Afterward, the image file in Info.plist.in.
if(APPLE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE "src/logo.png")
endif()

# Set C++ version
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_20)

# on Web targets, we need CMake to generate a HTML webpage. 
if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

# Configure SDL by calling its CMake file.
# we use EXCLUDE_FROM_ALL so that its install targets and configs don't
# pollute upwards into our configuration.
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# If you don't want SDL_ttf, then remove this section.
set(SDLTTF_VENDORED ON) # tell SDL_ttf to build its own dependencies
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)	

# SDL_mixer (used for playing audio)
set(SDLMIXER_MIDI_NATIVE OFF)     # disable formats we don't use to make the build faster and smaller. Also some of these don't work on all platforms so you'll need to do some experimentation.
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)     
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MOD_XMP OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_FLAC_LIBFLAC OFF)
set(SDLMIXER_VENDORED ON)   # tell SDL_mixer to build its own dependencies
add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)

# SDL_image (used for loading various image formats)
set(SDLIMAGE_VENDORED ON)
set(SDLIMAGE_AVIF OFF)	# disable formats we don't use to make the build faster and smaller.
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_JPEG OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_PNG_LIBPNG OFF)    # SDL3 includes built-in support for loading PNGs. If you need more than what that supports, enable this option.
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)

# Abseil (required by MediaPipe)
set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(external/abseil-cpp EXCLUDE_FROM_ALL)

# mediapipe Integration
# Add mediapipe include directory to enable includes like "mediapipe/framework/calculator_graph.h"
target_include_directories(${EXECUTABLE_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/abseil-cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/protobuf/src"
     "${CMAKE_CURRENT_SOURCE_DIR}/external/glog/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glog/include"
)

# Link mediapipe precompiled libraries based on platform
if(APPLE)
    if(IOS)
        # iOS platform - Link MediaPipeUnity.framework
        set(MEDIAPIPE_FRAMEWORK "${CMAKE_CURRENT_SOURCE_DIR}/external/mediapipe/libs/ios/MediaPipeUnity.framework")
        if(EXISTS ${MEDIAPIPE_FRAMEWORK})
            target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${MEDIAPIPE_FRAMEWORK})
            # Embed framework in the app bundle and set correct rpath
            set_target_properties(${EXECUTABLE_NAME} PROPERTIES
                XCODE_EMBED_FRAMEWORKS ${MEDIAPIPE_FRAMEWORK}
                XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "@executable_path/Frameworks"
            )
            message(STATUS "mediapipe framework linked: ${MEDIAPIPE_FRAMEWORK}")
        else()
            message(WARNING "mediapipe framework not found: ${MEDIAPIPE_FRAMEWORK}")
        endif()
    else()
        # macOS platform - Link libmediapipe_c.dylib
        set(MEDIAPIPE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/mediapipe/libs/osx")
        set(MEDIAPIPE_LIB_SOURCE "${MEDIAPIPE_LIB_DIR}/libmediapipe_c.dylib")
        set(MEDIAPIPE_LIB_FIXED "${CMAKE_BINARY_DIR}/libmediapipe_c_fixed.dylib")
        
        if(EXISTS ${MEDIAPIPE_LIB_SOURCE})
            # Copy and fix the MediaPipe library install name before linking
            add_custom_command(
                OUTPUT ${MEDIAPIPE_LIB_FIXED}
                COMMAND ${CMAKE_COMMAND} -E copy ${MEDIAPIPE_LIB_SOURCE} ${MEDIAPIPE_LIB_FIXED}
                COMMAND install_name_tool -id "@executable_path/libmediapipe_c.dylib" ${MEDIAPIPE_LIB_FIXED}
                DEPENDS ${MEDIAPIPE_LIB_SOURCE}
                COMMENT "Preparing MediaPipe library with fixed install name"
            )
            
            add_custom_target(fix_mediapipe_dylib ALL DEPENDS ${MEDIAPIPE_LIB_FIXED})
            add_dependencies(${EXECUTABLE_NAME} fix_mediapipe_dylib)
            
            target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${MEDIAPIPE_LIB_FIXED})
            
            # Allow undefined symbols to be resolved at runtime from MediaPipe dylib
            target_link_options(${EXECUTABLE_NAME} PRIVATE "-Wl,-undefined,dynamic_lookup")
            
            # Copy the fixed dylib to the app bundle and fix the executable's reference
            add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${MEDIAPIPE_LIB_FIXED}
                    $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/libmediapipe_c.dylib
                COMMAND install_name_tool -change 
                    "@rpath/libmediapipe_c.dylib"
                    "@executable_path/libmediapipe_c.dylib"
                    $<TARGET_FILE:${EXECUTABLE_NAME}>
                COMMENT "Copying MediaPipe library and fixing executable reference"
            )
            
            message(STATUS "MediaPipe library will be linked from: ${MEDIAPIPE_LIB_SOURCE}")
        else()
            message(WARNING "MediaPipe library not found: ${MEDIAPIPE_LIB_SOURCE}")
        endif()
    endif()
elseif(ANDROID)
    # Android platform - Link .so libraries based on architecture
    set(MEDIAPIPE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/mediapipe/libs/android/${ANDROID_ABI}")
    
    # Link MediaPipe and OpenCV using -L/-l so the linker records the bare SONAME
    # (using IMPORTED targets with full paths causes libmain.so to embed the full
    # path as DT_NEEDED, which the Android dynamic linker cannot resolve at runtime)
    target_link_options(${EXECUTABLE_NAME} PRIVATE
        "-L${MEDIAPIPE_LIB_DIR}"
        "-lmediapipe_jni"
        "-lopencv_java4"
    )
    
    # Copy libraries to jniLibs for packaging into APK
    set(JNI_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL/android-project/app/src/main/jniLibs/${ANDROID_ABI}")
    file(MAKE_DIRECTORY ${JNI_LIBS_DIR})
    
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MEDIAPIPE_LIB_DIR}/libmediapipe_jni.so"
            "${JNI_LIBS_DIR}/libmediapipe_jni.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MEDIAPIPE_LIB_DIR}/libopencv_java4.so"
            "${JNI_LIBS_DIR}/libopencv_java4.so"
        COMMENT "Copying MediaPipe libraries to jniLibs for APK packaging"
    )
    
    message(STATUS "MediaPipe Android libraries configured for ${ANDROID_ABI}")
endif()

# Link SDL to our executable. This also makes its include directory available to us. 
target_link_libraries(${EXECUTABLE_NAME} PUBLIC 
	SDL3_ttf::SDL3_ttf      # remove if you are not using SDL_ttf
	SDL3_mixer::SDL3_mixer  # remove if you are not using SDL_mixer
	SDL3_image::SDL3_image	# remove if you are not using SDL_image
	SDL3::SDL3_test         # SDL test utilities
    SDL3::SDL3              # If using satelite libraries, SDL must be the last item in the list. 
)

# NOTE: We do NOT link Abseil separately because MediaPipe library has Abseil embedded.
# Linking both would cause ABI conflicts and crashes.

# Dealing with assets
# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if (APPLE)
    # on Apple targets, the application bundle has a "resources" subfolder where we can place our assets.
    # SDL_GetBasePath() gives us easy access to that location.
    set(input_root "${CMAKE_CURRENT_LIST_DIR}/src")
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${input_root}" "${FILE}")
        get_filename_component(relpath "${relpath}" DIRECTORY)
        target_sources(${EXECUTABLE_NAME} PRIVATE ${FILE})
        set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relpath}")
    endmacro()
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/the_entertainer.ogg")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/gs_tiger.svg")
elseif(EMSCRIPTEN)
    # on the web, we have to put the files inside of the webassembly
    # somewhat unintuitively, this is done via a linker argument.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/the_entertainer.ogg@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/gs_tiger.svg@/\""
    )
else()
    if (ANDROID)
        if (NOT MOBILE_ASSETS_DIR)
            message(FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
        endif()
        file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}")    # we need to create the project Assets dir otherwise CMake won't copy our assets.
    endif()

    
    macro(copy_helper filename)
        if (ANDROID)
            # MOBILE_ASSETS_DIR is set in the gradle file via the cmake command line and points to the Android Studio Assets folder. 
            # when we copy assets there, the Android build pipeline knows to add them to the apk. 
            set(outname "${MOBILE_ASSETS_DIR}/${filename}")    
        else()
            # for platforms that do not have a good packaging format, all we can do is copy the assets to the process working directory.
            set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
        endif()
        add_custom_command(POST_BUILD
            TARGET "${EXECUTABLE_NAME}"
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/src/${filename}" "${outname}"
	    )
    endmacro()
    copy_helper("Inter-VariableFont.ttf")
    copy_helper("the_entertainer.ogg")
    copy_helper("gs_tiger.svg")
endif()

# set some extra configs for each platform
set(RESOURCE_FILES "src/logo.png")
if(IOS)
    set(RESOURCE_FILES "src/iosLaunchScreen.storyboard;src/logo.png") # make the splash screen show up on iOS
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch. 
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
	XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)

# Platform-specific code signing settings
if(IOS)
    # iOS requires code signing to install on devices
    # Set your Apple Developer Team ID here (find it at developer.apple.com)
    set(DEVELOPMENT_TEAM_ID "" CACHE STRING "Apple Developer Team ID for code signing")
    
    if(DEVELOPMENT_TEAM_ID)
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${DEVELOPMENT_TEAM_ID}"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
        )
        message(STATUS "iOS code signing enabled with team: ${DEVELOPMENT_TEAM_ID}")
    else()
        message(WARNING "No DEVELOPMENT_TEAM_ID set. iOS build will not be signable. Set with -DDEVELOPMENT_TEAM_ID=YourTeamID")
    endif()
else()
    # Disable code signing for macOS
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    )
endif()

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries 
# or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS ${EXECUTABLE_NAME}
    	BUNDLE DESTINATION ./install COMPONENT Runtime
   	    RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
	
    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    INSTALL(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        " 
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
