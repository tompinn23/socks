cmake_minimum_required(VERSION 3.15)

project(socks)

find_package(Threads)

#add_executable(socks src/socks.c src/net.c src/peer.c src/log.c src/mem.c)
add_executable(socks src/socks.cxx src/server.cxx src/log.cxx)
target_link_libraries(socks Threads::Threads)
