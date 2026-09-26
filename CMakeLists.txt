cmake_minimum_required(VERSION 3.6.3...3.10)

project(SRR2)

set(SRR2_TITLE "Simpsons Hit & Run")
set(SRR2_VERSION "00.61")
set(SRR2_FILENAME "Simpsons")
set(SRR2_REGION "NTSC" CACHE STRING "Select PAL for the EFIGS multi-language version")
set_property(CACHE SRR2_REGION PROPERTY STRINGS "NTSC" "PAL")

option(SRR2_USE_DLMALLOC "Use Doug Lea's memory allocator for fixed-size heaps" OFF)
option(SRR2_REROUTE_ALL_HEAP_ALLOCATIONS "Reroute all memory allocations by new operator to internal heaps" OFF)
option(SRR2_USE_BINK "Use RAD Game Tools Bink SDK instead of FFmpeg" OFF) #lo vuelvo a poner a off
option(SRR2_BUILD_TESTS "Build sample programs for internal libraries" ON)
option(SRR2_MICROPROFILE "Use microprofile instead of the internal profiler" OFF)
option(SRR2_P3D_CG "Use CG shaders rather than GLSL for OpenGL ES" OFF)



# Ahora detectamos la plataforma y ponemos el valor correcto por defecto
if(ANDROID)
	set(SRR2_P3D_PDDI "GLES2" CACHE STRING "Pure3D PDDI implementation")
	#set(SRR2_USE_BINK ON CACHE BOOL "Use RAD Game Tools Bink SDK instead of FFmpeg" FORCE)
	message(STATUS "Android detected - PDDI set to GLES2")
elseif(IOS)
	# iOS solo expone OpenGL ES 2.0 (via la capa SDL uikit). No hay OpenGL desktop ni EGL nativo.
	set(SRR2_P3D_PDDI "GLES2" CACHE STRING "Pure3D PDDI implementation")
	message(STATUS "iOS detected - PDDI set to GLES2")
elseif(VITA)
	set(SRR2_P3D_PDDI "GXM" CACHE STRING "Pure3D PDDI implementation")
	message(STATUS "Vita detected - PDDI set to GXM")
else()
	set(SRR2_P3D_PDDI "OpenGL" CACHE STRING "Pure3D PDDI implementation")
	message(STATUS "Desktop detected - PDDI set to OpenGL")
endif()



if(VITA)
	option(SRR2_USE_VITAGL "Use VitaGL as an OpenGL (ES) implementation" ON)
	set_property(CACHE SRR2_P3D_PDDI PROPERTY STRINGS "OpenGL" "GLES1" "GLES2" "GXM")
else()
	set_property(CACHE SRR2_P3D_PDDI PROPERTY STRINGS "OpenGL" "GLES1" "GLES2")
endif()



if(VITA)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	add_definitions(-DRAD_VITA)
	## Configuration options for this app
	# Display name (under bubble in LiveArea)
	set(VITA_APP_NAME ${SRR2_TITLE})
	# Unique ID must be exactly 9 characters. Recommended: XXXXYYYYY where X =
	# unique string of developer and Y = a unique number for this app
	set(VITA_TITLEID  "ROADRAGE2")
	# Optional version string to show in LiveArea's more info screen
	set(VITA_VERSION ${SRR2_VERSION})
endif()

# Android (define BEFORE add_subdirectory so libs/code see it)
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
	add_definitions(-DRAD_ANDROID)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog -landroid -lEGL -lGLESv2")
endif()

# iOS (define BEFORE add_subdirectory so libs/code see it)
if(IOS)
	add_definitions(-DRAD_IOS)
	# iOS es puramente estatico (no hay .so); GLES2 se enlaza via frameworks del SDK.
	set(CMAKE_OSX_SYSROOT iphoneos)
	message(STATUS "iOS detected - RAD_IOS defined")
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()


# SDL - Configuración por plataforma
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android" OR IOS)
    # Android/iOS: compilar SDL2 desde libs/SDL2 (el arbol vendido ya soporta uikit)
    message(STATUS "Mobile detected - Compilando SDL2 desde libs/SDL2")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/CMakeLists.txt")
        add_subdirectory(libs/SDL2 SDL2_Build EXCLUDE_FROM_ALL)
        set(SDL_LIBRARY SDL2::SDL2)
    else()
        message(FATAL_ERROR "SDL2 not found in libs/SDL2/CMakeLists.txt")
    endif()
else()
    # Desktop/Vita/etc: primero probar SDL3, si no, SDL2
    if(NOT VITA)
        find_package(SDL3 QUIET)
    endif()

    if(SDL3_FOUND)
        get_target_property(SDL_INCLUDE_DIR SDL3::Headers INTERFACE_INCLUDE_DIRECTORIES)
        include_directories(${SDL_INCLUDE_DIR}/SDL3)
        set(SDL_LIBRARY SDL3::SDL3)
        message(STATUS "SDL3 found in system")
    else()
        find_package(SDL2 REQUIRED)
        set(SDL_LIBRARY SDL2::SDL2 SDL2::SDL2main)
        message(STATUS "SDL2 found in system")
    endif()
endif()




# PNG / libpng
if(ANDROID OR IOS)
    # Compilar libpng desde tu carpeta
    add_subdirectory(libs/libpng EXCLUDE_FROM_ALL)
	
    # iOS no trae OpenAL del sistema; compilamos openal-soft como STATIC (CoreAudio backend).
    # Sin esto, el target OpenAL se crea como SHARED y el link/signing de iOS falla.
    if(IOS)
        set(LIBTYPE STATIC CACHE STRING "OpenAL build type (STATIC for iOS)" FORCE)
        set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
    endif()
	add_subdirectory(libs/openal-soft EXCLUDE_FROM_ALL)
    # zlib viene en Android (NDK); en iOS la trae el SDK (libz)
    find_library(ZLIB_LIB z)

    # Variables "compat" por si tus cmakes antiguos usan estas
    set(PNG_LIBRARY png_static)
    set(PNG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/libs/libpng")
else()
    find_package(PNG REQUIRED)
endif()


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(NOT ANDROID AND NOT IOS)
  find_package(OpenAL REQUIRED)
endif()


if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-fvisibility=hidden)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(
			-Werror
			-ggdb
		)
	endif()
	if(VITA)
		add_compile_options(
			-mtune=cortex-a9
			-mfpu=neon
		)
	endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DRAD_DEBUG)
else()
	add_definitions(-DRAD_RELEASE)
endif()

add_definitions(
	-DRAD_CONSOLE
	-DRAD_WIN32
	-DRAD_SDL
)

if(SRR2_REGION STREQUAL "PAL")
	add_definitions(-DPAL)
endif()
if(SRR2_USE_DLMALLOC)
	add_definitions(-DUSE_DOUG_LEA_HEAP)
endif()
if(SRR2_REROUTE_ALL_HEAP_ALLOCATIONS)
	add_definitions(-DREROUTE_ALL_HEAP_ALLOCATIONS)
endif()

if(SRR2_USE_BINK)
	add_definitions(-DRAD_MOVIEPLAYER_USE_BINK)
else()
	if(NOT ANDROID AND NOT IOS)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(FFmpeg REQUIRED IMPORTED_TARGET
				libavformat
				libavcodec
				libavutil
				libswresample
				libswscale
		)
	elseif(IOS)
		# iOS: FFmpeg 6.0 enlazado estaticamente (compilado aparte con el SDK del simulator/device).
		# Localizamos las bibliotecas construidas para iOS. Sobreescribible con -DSRR2_IOS_FFMPEG_DIR=...
		# El default apunta a donde ios-project/build-ffmpeg.sh deja los .a (fuera del repo, no se commitean).
		set(SRR2_IOS_FFMPEG_DIR "$ENV{HOME}/src/srr2_ios_builds/ffmpeg_out_sim" CACHE PATH "iOS FFmpeg static build (lib/, include/)")
		# Firmar la app para dispositivo real (iphoneos). OFF por defecto (simulador).
		set(SRR2_SIGN OFF CACHE BOOL "Sign the iOS app (release/device builds)")
		if(NOT EXISTS "${SRR2_IOS_FFMPEG_DIR}/lib/libavcodec.a")
			message(WARNING "iOS FFmpeg static libs not found in ${SRR2_IOS_FFMPEG_DIR}/lib (missing libavcodec.a). Movie playback will fail to link.")
		endif()

		# Creamos targets IMPORTED para cada biblioteca para enlazarlas como tal.
		foreach(lib avcodec avformat avutil swresample swscale)
			add_library(SRR2_FFMPEG_${lib} STATIC IMPORTED GLOBAL)
			set_target_properties(SRR2_FFMPEG_${lib} PROPERTIES
				IMPORTED_LOCATION "${SRR2_IOS_FFMPEG_DIR}/lib/lib${lib}.a"
			)
		endforeach()
		set(SRR2_FFMPEG_LIBS
			SRR2_FFMPEG_avcodec
			SRR2_FFMPEG_avformat
			SRR2_FFMPEG_avutil
			SRR2_FFMPEG_swresample
			SRR2_FFMPEG_swscale
		)
		set(FFMPEG_INCLUDE_DIR "${SRR2_IOS_FFMPEG_DIR}/include")
	else()
		# ✅ ANDROID: obtener FFmpeg desde Prefab (AAR com.fpliu...:ffmpeg)
    # Prefab expone un paquete CMake. Normalmente se encuentra con find_package().
    find_package(ffmpeg CONFIG REQUIRED)

    # ✅ Creamos una "lista" de targets/librerías para linkear después donde toque.
    # Nota: los nombres exactos pueden variar según el paquete; si falla, te digo cómo inspeccionarlos.
    set(SRR2_FFMPEG_LIBS
        ffmpeg::avcodec
        ffmpeg::avformat
        ffmpeg::avutil
        ffmpeg::swresample
        ffmpeg::swscale
		)
	endif()
endif()

if(SRR2_MICROPROFILE)
	add_definitions(-DMICROPROFILE_ENABLED)
endif()
if(SRR2_P3D_CG)
	add_definitions(-DRAD_CG)
endif()
if(SRR2_USE_VITAGL)
    add_definitions(-DRAD_VITAGL)
endif()
if(SRR2_P3D_PDDI STREQUAL "GXM")
    add_definitions(-DRAD_GXM)
endif()
if(SRR2_P3D_PDDI STREQUAL "GLES1")
	add_definitions(-DRAD_GLES)
	add_definitions(-DRAD_GLES_VERSION=1)
endif()
if(SRR2_P3D_PDDI STREQUAL "GLES2")
	add_definitions(-DRAD_GLES)
	add_definitions(-DRAD_GLES_VERSION=2)
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# iOS: frameworks del sistema que el engine usa (GLES2, audio CoreAudio, UIKit para la app shell)
if(IOS)
	find_library(OPENGLES_FRAMEWORK OpenGLES)
	find_library(UIKit_FRAMEWORK UIKit)
	find_library(CoreAudio_FRAMEWORK CoreAudio)
	find_library(AudioToolbox_FRAMEWORK AudioToolbox)
	find_library(CoreFoundation_FRAMEWORK CoreFoundation)
	find_library(QuartzCore_FRAMEWORK QuartzCore)
	find_library(Metal_FRAMEWORK Metal)
	find_library(Foundation_FRAMEWORK Foundation)
endif()

add_subdirectory(libs)
add_subdirectory(code)

if(VITA)
	## Create Vita files
	if(SRR2_P3D_PDDI STREQUAL "GLES2" AND NOT SRR2_USE_VITAGL)
		vita_create_self(${PROJECT_NAME}.self code/${PROJECT_NAME} UNSAFE)
		# The FILE directive lets you add additional files to the VPK, the syntax is
		# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
		vita_create_vpk(${SRR2_FILENAME}${SRR2_REGION}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
			VERSION ${VITA_VERSION}
			NAME ${VITA_APP_NAME}
			FILE sce_sys/icon0.png sce_sys/icon0.png
			sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
			sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
			sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
			art art

			#suprx
			module/libGLESv2.suprx module/libGLESv2.suprx
			module/libgpu_es4_ext.suprx module/libgpu_es4_ext.suprx
			module/libIMGEGL.suprx module/libIMGEGL.suprx
			module/libpvrPSP2_WSEGL.suprx module/libpvrPSP2_WSEGL.suprx
		)
	else()
		vita_create_self(${PROJECT_NAME}.self code/${PROJECT_NAME})
		# The FILE directive lets you add additional files to the VPK, the syntax is
		# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
		vita_create_vpk(${SRR2_FILENAME}${SRR2_REGION}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
			VERSION ${VITA_VERSION}
			NAME ${VITA_APP_NAME}
			FILE sce_sys/icon0.png sce_sys/icon0.png
			sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
			sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
			sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
			art art

			#gxp
			libs/pure3d/pddi/gxm/shaders/vertex_cg.gxp shaders/vertex_cg.gxp
			libs/pure3d/pddi/gxm/shaders/color_cg.gxp shaders/color_cg.gxp
			libs/pure3d/pddi/gxm/shaders/texture_cg.gxp shaders/texture_cg.gxp
			libs/pure3d/pddi/gxm/shaders/alpha_cg.gxp shaders/alpha_cg.gxp
		)
	endif()
endif()



if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
	nx_generate_nacp(OUTPUT ${PROJECT_NAME}.nacp
		NAME ${SRR2_TITLE}
		AUTHOR "Radical Entertainment"
		VERSION ${SRR2_VERSION}
	)
	nx_create_nro(code/${PROJECT_NAME}
		OUTPUT ${SRR2_FILENAME}${SRR2_REGION}.nro
		ICON ${PROJECT_NAME}.jpg
		NACP ${PROJECT_NAME}.nacp
		ROMFS romfs
	)
endif()
