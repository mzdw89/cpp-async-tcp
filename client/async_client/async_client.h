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

#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>

#include "../../shared/packets/packets.h"

// TODO: 
// -add handshake timeout so we don't wait infinitely
// -check if connected_ needs a mutex
// -fix the stupid lag on handshake (why tf does it happen???)

namespace fi {
	class async_tcp_client {
	public:
		async_tcp_client( );
		~async_tcp_client( );

		bool connect( std::string_view ip, std::string_view port );
		void disconnect( );

		bool is_connected( );

		void send_packet( packets::base_packet* const packet );

		// The callback will be called once a packet is received.
		// You must register your callback before you connect to
		// the server, as not doing so will result in an exception.
		void register_callback( std::function< void( async_tcp_client* const, const packets::packet_id, packets::detail::binary_serializer& ) > callback_fn );

		// This function will be called as soon as the client disconnects or has been disconnected from the server.
		void register_disconnect_callback( std::function< void( async_tcp_client* const ) > callback_fn );

	private:
	#ifdef _WIN32
		WSADATA wsa_data_ = { };
	#endif // _WIN32
		
		packets::header construct_packet_header( packets::packet_length length, packets::packet_id id, packets::packet_flags flags );

		// We have a seperate function which will perform a handshake with the server
		// to make sure we are talking to a server which will understand our packets.
		bool perform_handshake( );

		// Function for sending our packet
		bool send_packet_internal( void* const data, const packets::packet_length length );

		// Handles disconnecting
		enum class disconnect_reasons : std::uint8_t {
			reason_handshake_fail = 0,
			reason_error, 
			reason_stop,
			reason_server_stop
		};

		void disconnect_internal( const disconnect_reasons reason );

		// These functions are running in a thread
		void process_data( );
		void receive_data( );

		bool connected_ = false;

		// This specifies the buffer size when receiving data. 
		// It does not affect the size of the processing queue.
		const std::uint32_t buffer_size_ = PACKET_BUFFER_SIZE;

		SOCKET socket_ = 0;

		std::mutex disconnect_mtx_ = { }, process_mtx_ = { }, send_mtx_ = { };

		std::vector< std::uint8_t > process_buffer_ = { };

		std::function< void( async_tcp_client* const ) > on_disconnect_callback_ = { };
		std::function< void( async_tcp_client* const, const packets::packet_id, packets::detail::binary_serializer& ) > process_callback_ = { };

		std::thread processing_thread_ = { }, receiving_thread_ = { };

		// This will help us in serializing our packet data
		packets::detail::binary_serializer serializer = { };

	public:
		class exception : public std::exception {
		public:
			enum reason_id : std::uint8_t {
				none = 0,
				wsastartup_failure,
				already_connected,
				getaddrinfo_failure,
				socket_failure,
				connection_error,
				packet_nullptr,
				null_callback,
				no_callback

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