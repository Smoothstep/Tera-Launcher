cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(tera-launcher)
set (VERSION "0.1")

set(sources
	allocation
	bdecode
	cef
	file_info
	handler
	launcher
	main
	patcher
	patcher_callback
	patcher_service
	xml_manifest_parser
)

set(headers
	allocation
	bdecode
	cef
	config
	crypt
	error_codes
	extension
	file_info
	handler
	launcher
	log
	mapped_file
	parser
	patcher
	patcher_callback
	patcher_service
	resource
	tera_zip_format
	torrent
	xml_manifest_parser
	utils/pool
	utils/pool_win
	utils/function
	)
	
set(config
	bg.jpg
	Launcher.ico
	launcher.ini
	login.html
	patcher.ini
	)

include_directories(include/)
link_directories(lib/)

foreach(s ${sources})
	list(APPEND sources2 src/${s})
endforeach(s)

foreach(s ${headers})
	list(APPEND headers2 include/${s})
endforeach(s)

foreach(s ${config})
	list(APPEND config2 config/${s})
endforeach(s)

add_executable(tera-launcher ${sources2} ${headers2} ${CMAKE_CURRENT_SOURCE_DIR}/rc/Launcher.rc)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARY_DIRS CACHE)

if(NOT DEFINED Boost_INCLUDE_DIR OR NOT DEFINED Boost_LIBRARIES)
	FIND_PACKAGE(Boost REQUIRED COMPONENTS system filesystem regex thread atomic iostreams container graph chrono serialization)
endif()

include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(tera-launcher ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

file(COPY config/ DESTINATION config)
file(COPY bin/ DESTINATION config/bin)

target_link_libraries(tera-launcher zlib.lib)
target_link_libraries(tera-launcher xz.lib)
target_link_libraries(tera-launcher xdelta3.lib)
target_link_libraries(tera-launcher libcef.lib)
target_link_libraries(tera-launcher libcef_dll_wrapper.lib)
target_link_libraries(tera-launcher KtmW32.lib)

if (WIN32)
	add_definitions(-D_WIN32_WINNT=0x0600)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	if (MSVC)
		add_definitions(-DBOOST_ALL_NO_LIB)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	endif()
endif()

if (exceptions)
	if (MSVC)
		add_definitions(/EHsc)
	else (MSVC)
		add_definitions(-fexceptions)
	endif (MSVC)
else()
	if (MSVC)
		add_definitions(-D_HAS_EXCEPTIONS=0)
	else (MSVC)
		add_definitions(-fno-exceptions)
	endif (MSVC)
endif()

add_definitions(-UNICODE)
add_definitions(-D_UNICODE)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)

get_property (COMPILETIME_OPTIONS_LIST
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIRECTORY}
	PROPERTY COMPILE_DEFINITIONS
	)
foreach (s ${COMPILETIME_OPTIONS_LIST})
	set (COMPILETIME_OPTIONS "${COMPILETIME_OPTIONS} -D${s}")
endforeach (s)

string (COMPARE EQUAL "${CMAKE_SIZEOF_VOID_P}" "8" IS64BITS)

if (IS64BITS AND RESPECTLIB64)
	set (LIBDIR "lib64")
else()
	set (LIBDIR "lib")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

install(TARGETS tera-launcher DESTINATION ${LIBDIR})
install(DIRECTORY include/
	DESTINATION includes
	PATTERN ".svn" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tera-launcher.pc DESTINATION ${LIBDIR}/pkgconfig)

