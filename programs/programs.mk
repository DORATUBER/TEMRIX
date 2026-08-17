PROGRAMS := Init Fs Compositor Terminal NetworkServer KeyboardServer Gpu ExampleProgram

Init_SRCS := \
	Init/Init.cpp

Fs_SRCS := \
	Fs/Fs.cpp

Compositor_SRCS := \
	Compositor/Compositor.cpp \
	Graphics/font.cpp

Terminal_SRCS := \
	Terminal/Terminal.cpp \
	Terminal/Shell.cpp \
	Terminal/TermGrid.cpp \
	Terminal/TextEditor.cpp \
	Graphics/font.cpp

NetworkServer_SRCS := \
	Network/NetworkServer.cpp \
	Graphics/font.cpp \
	include/net/link/arp.cpp \
	include/net/link/ethernet.cpp \
	include/net/internet/ip.cpp \
	include/net/transport/udp.cpp \
	include/net/transport/tcp.cpp \
	include/net/services/dns/dns.cpp

KeyboardServer_SRCS := \
	Keyboard/KeyboardServer.cpp

Gpu_SRCS := \
	Gpu/Gpu.cpp

ExampleProgram_SRCS := \
	Example/ExampleProgram.cpp \
	Graphics/font.cpp

