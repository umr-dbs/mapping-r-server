include( LibFindMacros )

find_package( R REQUIRED )
message( STATUS "R_INCLUDE_DIR = ${R_INCLUDE_DIR}" )

set( RINSIDE_FOUND 0 )
execute_process(COMMAND ${Rscript_COMMAND} -e "Rcpp:::CxxFlags()"
        OUTPUT_VARIABLE RCPP_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
if ( ${RCPP_FLAGS} MATCHES "[-][I]([^ ;])+" )
    string( SUBSTRING ${CMAKE_MATCH_0} 2 -1 _INCL )
    set( RCPP_INCLUDE_DIR ${_INCL} )
    message( STATUS "RCPP_INCLUDE_DIR = ${RCPP_INCLUDE_DIR}" )
endif( )

execute_process(COMMAND ${Rscript_COMMAND} -e "RInside:::CxxFlags()"
        OUTPUT_VARIABLE RINSIDE_CPPFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
message( STATUS "RINSIDE_CPPFLAGS = ${RINSIDE_CPPFLAGS}" )
if ( ${RINSIDE_CPPFLAGS} MATCHES "[-][I](.+)( -|$)" )
    string( SUBSTRING ${CMAKE_MATCH_0} 2 -1 _INCL )
    set( RINSIDE_INCLUDE_DIR ${_INCL} )
    message( STATUS "RINSIDE_INCLUDE_DIR = ${RINSIDE_INCLUDE_DIR}" )
endif( )

execute_process( COMMAND ${Rscript_COMMAND} -e "RInside:::LdFlags()"
        OUTPUT_VARIABLE RINSIDE_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE )
if ( WIN32 )
    set( re "^\"(.+)\"$" )
    string( REGEX MATCH ${re} matched ${RINSIDE_LDFLAGS} )
    if( matched )
        set( RINSIDE_LIBRARY ${CMAKE_MATCH_1} )
    else( matched )
        set( RINSIDE_LIBRARY ${RINSIDE_LDFLAGS} )
    endif( matched )
    message( STATUS "RINSIDE_LIBRARY = ${RINSIDE_LIBRARY}" )
    get_filename_component( RINSIDE_LIBRARY_PATH ${RINSIDE_LIBRARY} PATH )
    message( STATUS "RINSIDE_LIBRARY_PATH = ${RINSIDE_LIBRARY_PATH}" )
else( WIN32 )
    if ( ${RINSIDE_LDFLAGS} MATCHES "[-][L]([^ ;])+" )
        string( SUBSTRING ${CMAKE_MATCH_0} 2 -1 _INCL )
        set( RINSIDE_LIBRARY_DIR ${_INCL} )
        message( STATUS "RINSIDE_LIBRARY_DIR = ${RINSIDE_LIBRARY_DIR}" )
        find_library( RINSIDE_LIBRARY RInside
                PATHS ${RINSIDE_LIBRARY_DIR}
                NO_DEFAULT_PATH )
        message( STATUS "RINSIDE_LIBRARY = ${RINSIDE_LIBRARY}" )
    endif( )
endif( WIN32 )

set( RINSIDE_PROCESS_INCLUDES
        RCPP_INCLUDE_DIR RINSIDE_INCLUDE_DIR R_INCLUDE_DIR )
set( RINSIDE_PROCESS_LIBS RINSIDE_LIBRARY R_LIBRARY_BASE )
libfind_process( RINSIDE )

message( STATUS "RINSIDE_INCLUDE_DIRS = ${RINSIDE_INCLUDE_DIRS}" )
message( STATUS "RINSIDE_LIBRARIES = ${RINSIDE_LIBRARIES}" )