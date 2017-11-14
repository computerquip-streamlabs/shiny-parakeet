set(ASN_CLIENT_SRC 
	${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
)

add_executable(asn-client ${ASN_CLIENT_SRC})
target_link_libraries(asn-client asio)