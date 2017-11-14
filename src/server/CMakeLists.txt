set(ASN_SERVER_SRC 
	${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
)

add_executable(asn-server ${ASN_SERVER_SRC})
target_link_libraries(asn-server asio)