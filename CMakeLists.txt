cmake_minimum_required(VERSION 3.22)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckIPOSupported)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(
  Zquake
  VERSION 0.16
  DESCRIPTION "The ZQuake quake world client"
  LANGUAGES C CXX)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
else()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
set(ZQUAKE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cd_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_cam.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_cmd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_draw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_effects.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_ents.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_input.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_nqdemo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_parse.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_pred.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_sbar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_screen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_tent.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_view.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmodel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/com_msg.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/common.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/console.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/crc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cvar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_draw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_mesh.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_ngraph.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_ralias.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_refrag.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_rlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_rmain.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_rmisc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_rsprite.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_rsurf.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_texture.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/gl_warp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/host.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/in_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/keys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mathlib.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mdfour.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_chan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_wins.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/nonintel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmove.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmovetst.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_cmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_edict.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_exec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/q_shared.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/rc_image.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/rc_wad.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/skin.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/snd_fmod.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_authlists.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_bot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ccmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ents.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_init.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_master.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_move.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_nchan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_phys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_save.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_send.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_user.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_world.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sys_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/teamplay.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/version.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/vid_wgl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/zone.c")
set(ZQUAKE_VIDNULL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cd_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_cam.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_cmd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_draw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_effects.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_ents.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_input.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_nqdemo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_parse.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_pred.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_sbar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_screen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_tent.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_view.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmodel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/com_msg.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/common.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/console.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/crc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cvar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_draw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_mesh.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_ngraph.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_ralias.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_refrag.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_rlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_rmain.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_rmisc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_rsprite.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_rsurf.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_texture.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/gl_warp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/host.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/in_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/keys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mathlib.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mdfour.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_chan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_wins.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/nonintel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmove.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmovetst.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_cmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_edict.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_exec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/q_shared.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/rc_image.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/rc_wad.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/skin.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/snd_fmod.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_authlists.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_bot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ccmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ents.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_init.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_master.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_move.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_nchan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_phys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_save.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_send.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_user.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_world.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sys_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/teamplay.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/version.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vidnull/vid_wgl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/zone.c")
set(ZQDS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cl_null.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cmodel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/com_msg.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/common.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/crc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cvar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/host.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mathlib.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mdfour.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_chan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net_wins.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmove.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pmovetst.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_cmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_edict.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/pr_exec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/q_shared.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_authlists.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_bot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ccmds.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_ents.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_init.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_master.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_move.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_nchan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_phys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_send.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_user.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sv_world.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/sys_win.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/version.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/zone.c")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
find_package(FMOD MODULE REQUIRED)
find_package(OpenGL REQUIRED)
find_library(ONECORE_LIB onecore)
if(ONECORE_LIB)
  set(ZQUAKE_LIBS ${ONECORE_LIB} OpenGL::GL dxguid winmm)
  set(ZQDS_LIBS ${ONECORE_LIB})
else()
  set(ZQUAKE_LIBS OpenGL::GL dxguid wsock32 winmm)
  set(ZQDS_LIBS wsock32 winmm)
endif()
check_ipo_supported()
add_executable(zquake WIN32 ${ZQUAKE_SOURCES})
set_property(TARGET zquake PROPERTY OUTPUT_NAME zquake-gl)
if(MSVC)
  target_compile_options(zquake PRIVATE /W4 /utf-8 /bigobj)
else()
  target_compile_options(zquake PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  target_compile_definitions(zquake PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_include_directories(zquake PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_link_libraries(zquake PRIVATE ${ZQUAKE_LIBS} FMOD::FMOD)
target_compile_definitions(zquake PRIVATE AGRIP MAUTH GLQUAKE _WINDOWS)
set_property(TARGET zquake PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
add_executable(zquake_vidnull WIN32 ${ZQUAKE_VIDNULL_SOURCES})
set_property(TARGET zquake_vidnull PROPERTY OUTPUT_NAME zquake-vidnull)
if(MSVC)
  target_compile_options(zquake_vidnull PRIVATE /W4 /utf-8 /bigobj)
else()
  target_compile_options(zquake_vidnull PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  target_compile_definitions(zquake_vidnull PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_include_directories(zquake_vidnull
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_link_libraries(zquake_vidnull PRIVATE ${ZQUAKE_LIBS} FMOD::FMOD)
target_compile_definitions(zquake_vidnull PRIVATE AGRIP MAUTH VIDNULL _WINDOWS)
set_property(TARGET zquake_vidnull PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
add_executable(zqds ${ZQDS_SOURCES})
set_property(TARGET zqds PROPERTY OUTPUT_NAME zqds)
if(MSVC)
  target_compile_options(zqds PRIVATE /W4 /utf-8 /bigobj)
else()
  target_compile_options(zqds PRIVATE -Wall -Wextra)
endif()
if(WIN32)
  target_compile_definitions(zqds PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_include_directories(zqds PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_link_libraries(zqds PRIVATE ${ZQDS_LIBS})
target_compile_definitions(zqds PRIVATE AGRIP MAUTH SERVERONLY _CONSOLE)
set_property(TARGET zqds PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
