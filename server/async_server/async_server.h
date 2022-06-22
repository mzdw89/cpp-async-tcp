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

#include "../../shared/packets/packets.h"

namespace fi {
	class async_tcp_server {
	public:
		async_tcp_server( );
		~async_tcp_server( );

		void start( std::string_view port );
		void stop( );

		void disconnect_client( SOCKET who );

		bool is_running( );

		void send_packet( SOCKET to, packets::base_packet* packet );

		// The callback will be called once a packet is received. You must register
		// your callback before you start the server, as not doing so will result
		// in an exception.
		void register_callback( std::function< void( async_tcp_server* const, const SOCKET, const packets::packet_id, packets::detail::binary_serializer& ) > callback_fn );

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
		void run_heartbeat( ); 

		bool running_ = false;

		// This specifies the buffer size when receiving data. 
		// It does not affect the size of the processing queue.
		const std::uint32_t buffer_size_ = PACKET_BUFFER_SIZE;

		// The amount of time to wait between heartbeat packets
		const std::chrono::duration< long long > heartbeat_interval_ = std::chrono::seconds( 5 );

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

		// Our main processing callback
		std::function< void( async_tcp_server* const, const SOCKET, const packets::packet_id, packets::detail::binary_serializer& ) > process_callback_ = { };

		std::thread accepting_thread_ = { }, processing_thread_ = { }, receiving_thread_ = { }, heartbeat_thread_ { };

		// This will help us in serializing our packet data
		packets::detail::binary_serializer serializer = { };

	public:
		class exception : public std::exception {
		public:
			enum reason_id : std::uint8_t {
				none = 0,
				wsastartup_failure,
				already_running,
				getaddrinfo_failure,
				socket_failure,
				packet_nullptr,
				null_callback,
				no_callback,
				bind_error,
				listen_error
			};

			exception( reason_id reason, std::string_view what ) : reason_( reason ), what_( what ) { };

			virtual const char* what( ) const noexcept {
				return what_.data( );
			}

			const reason_id get_reason( ) {
				return reason_;
			}

		private:
			std::string what_ = { };
			reason_id reason_ = reason_id::none;
		};
	};
} // namespace fi