cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(FF4.elf C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(sndfile REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(sdl REQUIRED IMPORTED_TARGET sdl)
pkg_check_modules(vorbisfile REQUIRED IMPORTED_TARGET vorbisfile)
set(VITA_APP_NAME "Final Fantasy IV")
set(VITA_TITLEID  "FF4000000")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q,--wrap,memcpy,--wrap,memmove,--wrap,memset -Wall -O3 -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wno-write-strings")

add_executable(FF4.elf
  loader/main.c
  loader/dialog.c
  loader/so_util.c
  loader/bridge.c
  loader/stb_image.c
  loader/stb_truetype.c
  loader/trophies.c
  loader/base64.cpp
)

if(CMAKE_VERSION VERSION_LESS "3.20.0")
  add_dependencies(FF4.elf companion.bin)
else()
  add_dependencies(FF4.elf companion.bin-self)
endif()


target_link_libraries(FF4.elf
  -pthread
  -Wl,--start-group
  PkgConfig::sndfile
  PkgConfig::sdl
  PkgConfig::vorbisfile
  m
  stdc++
  vitaGL
  vitashark
  SceShaccCgExt
  taihen_stub
  mathneon
  kubridge_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceNpTrophy_stub
  SceAvPlayer_stub
  OpenSLES
  sndfile
  FLAC
  vorbisfile
  vorbis
  vorbisenc
  ogg
  SDL
  SceHid_stub
  z
  -Wl,--end-group
)

add_executable(companion
  companion/main.cpp
)

target_link_libraries(companion
  -Wl,--start-group
  imgui
  vitaGL
  vitashark
  SceShaccCgExt
  taihen_stub
  mathneon
  SceAppMgr_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceVshBridge_stub
  SceSysmodule_stub
  SceTouch_stub
  SceAppUtil_stub
  -Wl,--end-group
)

vita_create_self(companion.bin companion UNSAFE)

vita_create_self(eboot.bin FF4.elf UNSAFE)
vita_create_vpk(FF4.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg0.png sce_sys/livearea/contents/bg0.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/config.png sce_sys/livearea/contents/config.png
       ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
	   ${CMAKE_SOURCE_DIR}/sce_sys/trophy/FFIV00001_00/TROPHY.TRP sce_sys/trophy/FFIV00001_00/TROPHY.TRP
       ${CMAKE_SOURCE_DIR}/NotoSansJP-Regular.ttf NotoSansJP-Regular.ttf
       ${CMAKE_SOURCE_DIR}/NotoSansSC-Regular.ttf NotoSansSC-Regular.ttf
       ${CMAKE_SOURCE_DIR}/NotoSansKR-Regular.ttf NotoSansKR-Regular.ttf
       ${CMAKE_BINARY_DIR}/companion.bin companion.bin
       ${CMAKE_SOURCE_DIR}/shaders/1_Negative_f.cg shaders/1_Negative_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/1_Negative_v.cg shaders/1_Negative_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_f.cg shaders/2_FXAA_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/2_FXAA_v.cg shaders/2_FXAA_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_f.cg shaders/3_Sepia_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/3_Sepia_v.cg shaders/3_Sepia_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_f.cg shaders/4_Greyscale_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/4_Greyscale_v.cg shaders/4_Greyscale_v.cg
       ${CMAKE_SOURCE_DIR}/shaders/5_CRT_f.cg shaders/5_CRT_f.cg
       ${CMAKE_SOURCE_DIR}/shaders/5_CRT_v.cg shaders/5_CRT_v.cg
)

set_property(SOURCE loader/so_util.c APPEND PROPERTY COMPILE_OPTIONS -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types)
