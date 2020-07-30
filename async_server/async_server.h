#pragma once

#pragma region os_dependent_includes

#ifdef _WIN32	// Windows Machine

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#elif __linux__	// Linux machine

// todo: add linux includes
#error Linux support not yet implemented (missing includes, untested on Linux).

#else
#error OS unknown or not supported.
#endif // _WIN32

#pragma endregion os_dependent_includes

#include <thread>
#include <unordered_map>
#include <mutex>
#include <functional>

#include "../packets/packets.h"

namespace fi {
	class async_tcp_server {
	public:
		async_tcp_server( );
		~async_tcp_server( );

		typedef std::function< void( async_tcp_server* const, const SOCKET, packets::detail::binary_serializer& ) > packet_callback_server_fn;

		void start( std::string_view port );
		void stop( );

		void disconnect_client( SOCKET who );

		bool is_running( );

		void send_packet( SOCKET to, packets::base_packet* packet );

		// The callback will be called once a packet with the corresponding ID is received.
		// You should always register your callbacks before you start the server, as doing
		// so after may result in packets being lost.
		void register_callback( packets::packet_id packet_id, packet_callback_server_fn callback_fn );
		void remove_callback( packets::packet_id packet_id );

		// If you have a lot of callbacks, it may be inefficient to call register_callback
		// for each packet. You can predefine a map yourself and set it with this funciton.
		// Please note that this will remove all existing callbacks.
		void set_callback_map( const std::unordered_map< packets::packet_id, packet_callback_server_fn >& callback_map );

		// This function will be called as soon as the server stops.
		void register_stop_callback( std::function< void( async_tcp_server* const ) > callback_fn );

		// This function will be called as soon as a client dis/connects from/to the server.
		void register_connect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn );
		void register_disconnect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn );

	private:
	#ifdef _WIN32
		WSADATA wsa_data_ = { };
	#endif // _WIN32

		packets::header construct_packet_header( packets::packet_length length, packets::packet_id id, packets::packet_flags flags );

		// We have a seperate function which will perform a handshake with the client
		// to make sure we are talking to a client which will understand our packets.
		bool perform_handshake( SOCKET with );

		// Function for sending our packet
		bool send_packet_internal( SOCKET to, void* const data, const packets::packet_length length );

		// These functions are running in a thread
		void accept_clients( );
		void process_data( );
		void receive_data( );

		bool running_ = false;

		// This specifies the buffer size when receiving data. 
		// It does not affect the size of the processing queue.
		const std::uint32_t buffer_size_ = PACKET_BUFFER_SIZE;

		SOCKET server_socket_ = 0;
	
		std::mutex send_mtx_ = { }, disconnect_mtx_ = { };

		// These CAN be accessed in the same thread multiple times, therefore we 
		// need to make these recursive.
		std::recursive_mutex client_mtx_ = { }, process_mtx_ = { };

		std::vector< SOCKET > clients_to_disconnect_ = { };

		std::vector< SOCKET > connected_clients_ = { };
		std::unordered_map< SOCKET, std::vector< std::uint8_t > > process_buffers_ = { };

		std::function< void( async_tcp_server* const, const SOCKET ) > on_connect_callback = { }, on_disconnect_callback_ = { };
		std::function< void( async_tcp_server* const ) > on_stop_callback_ = { };

		// Our list of packet callbacks
		std::unordered_map< packets::packet_id, packet_callback_server_fn > callbacks_ = { };

		std::thread accepting_thread_ = { }, processing_thread_ = { }, receiving_thread_ = { };

		// This will help us in serializing our packet data
		packets::detail::binary_serializer serializer = { };
	};
} // namespace fi