#include "async_server.h"

using namespace fi;

// TODO:
// -add handshake timeout
async_tcp_server::async_tcp_server( ) {
#ifdef _WIN32
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa_data_ ) != 0 )
		throw std::exception( "async_tcp_server::async_tcp_server: WSAStartup failed" );
#endif // _WIN32
}

async_tcp_server::~async_tcp_server( ) {
	stop( );

	if ( accepting_thread_.joinable( ) )
		accepting_thread_.join( );

	if ( processing_thread_.joinable( ) )
		processing_thread_.join( );

	if ( receiving_thread_.joinable( ) )
		receiving_thread_.join( );

#ifdef _WIN32
	WSACleanup( );
#endif // _WIN32
}

void async_tcp_server::start( std::string_view port ) {
	if ( running_ )
		throw std::exception( "async_tcp_server::start: attempted to start server while it was running" );

	addrinfo hints = { }, *result = nullptr;

	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	if ( getaddrinfo( nullptr, port.data( ), &hints, &result ) != 0 )
		throw std::exception( "async_tcp_server::start: getaddrinfo error" );

	server_socket_ = ::socket( result->ai_family, result->ai_socktype, result->ai_protocol );

	if ( server_socket_ == INVALID_SOCKET ) {
		freeaddrinfo( result );
		throw std::exception( "async_tcp_server::start: failed to create socket" );
	}

	if ( bind( server_socket_, result->ai_addr, result->ai_addrlen ) == SOCKET_ERROR ) {
		freeaddrinfo( result );
		throw std::exception( "async_tcp_server::start: failed to bind socket" );
	}

	if ( listen( server_socket_, SOMAXCONN ) == SOCKET_ERROR ) {
		freeaddrinfo( result );
		throw std::exception( "async_tcp_server::start: failed to listen on socket" );
	}

	freeaddrinfo( result );

	running_ = true;

	accepting_thread_ = std::thread( &async_tcp_server::accept_clients, this );
	processing_thread_ = std::thread( &async_tcp_server::process_data, this );
	receiving_thread_ = std::thread( &async_tcp_server::receive_data, this );
}

void async_tcp_server::stop( ) {
	if ( running_ ) {
		shutdown( server_socket_, SD_BOTH );
		closesocket( server_socket_ );

		running_ = false;

		if ( on_stop_callback_ )
			on_stop_callback_( this );

		connected_clients_.clear( );
	}
}

void async_tcp_server::disconnect_client( SOCKET who ) {
	std::lock_guard guard( disconnect_mtx_ );

	clients_to_disconnect_.push_back( who );
}

bool async_tcp_server::is_running( ) {
	return running_;
}

void async_tcp_server::send_packet( SOCKET to, packets::base_packet* packet ) {
	if ( !packet )
		throw std::exception( "async_tcp_server::send_packet: packet was nullptr" );

	std::lock_guard guard( send_mtx_ );

	serializer.reset( );

	// Serialize our data
	packet->serialize( serializer );

	// Allocate a buffer for our packet
	std::vector< std::uint8_t > packet_data( sizeof( packets::header ) + serializer.get_serialized_data_length( ) );

	// Construct our packet header
	packets::header packet_header = construct_packet_header(
		serializer.get_serialized_data_length( ),
		packet->get_id( ),
		packets::flags::fl_none
	);

	// Write our packet into the buffer
	memcpy( packet_data.data( ), &packet_header, sizeof( packets::header ) );

	memcpy(
		packet_data.data( ) + sizeof( packets::header ),
		serializer.get_serialized_data( ),
		serializer.get_serialized_data_length( )
	);

	// Attempt to send the packet
	if ( !send_packet_internal( to, packet_data.data( ), packet_data.size( ) ) )
		disconnect_client( to );
}

void async_tcp_server::register_callback( packets::packet_id packet_id, packet_callback_server_fn callback_fn ) {
	if ( !callback_fn )
		throw std::exception( "async_tcp_server::register_callback: no callback given" );

	callbacks_[ packet_id ] = callback_fn;
}

void async_tcp_server::remove_callback( packets::packet_id packet_id ) {
	callbacks_.erase( packet_id );
}

void async_tcp_server::set_callback_map( const std::unordered_map< packets::packet_id, packet_callback_server_fn >& callback_map ) {
	callbacks_ = callback_map;
}

void async_tcp_server::register_stop_callback( std::function< void( async_tcp_server* const ) > callback_fn ) {
	on_stop_callback_ = callback_fn;
}

void async_tcp_server::register_connect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn ) {
	on_connect_callback = callback_fn;
}

void async_tcp_server::register_disconnect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn ) {
	on_disconnect_callback_ = callback_fn;
}

packets::header async_tcp_server::construct_packet_header( packets::packet_length length, packets::packet_id id, packets::packet_flags flags ) {
	packets::header packet_header = { };

	packet_header.flags = flags;
	packet_header.id = id;
	packet_header.length = sizeof( packets::header ) + length;
	packet_header.magic = PACKET_MAGIC;

	return packet_header;
}

bool async_tcp_server::perform_handshake( SOCKET with ) {
	packets::header packet_header = construct_packet_header( 0, packets::ids::id_handshake, packets::flags::fl_handshake_sv );

	auto buffer = reinterpret_cast< char* >( &packet_header );

	// Send our header with no body and the handshake_sv flag
	if ( !send_packet_internal( with, &packet_header, sizeof( packets::header ) ) )
		return false;

	// Receive a response back. Should be the header with handshake_cl flag
	int bytes_received = 0;
	do {
		int received = recv( with, buffer + bytes_received, sizeof( packets::header ) - bytes_received, 0 );

		if ( received <= 0 )
			return false;

		bytes_received += received;
	} while ( bytes_received < sizeof( packets::header ) );

	// Check the header information for the information we are expecting
	if ( packet_header.flags != packets::flags::fl_handshake_cl )
		return false;

	if ( packet_header.id != packets::ids::id_handshake )
		return false;

	if ( packet_header.length != sizeof( packets::header ) )
		return false;

	if ( packet_header.magic != PACKET_MAGIC )
		return false;

	return true;
}

bool async_tcp_server::send_packet_internal( SOCKET to, void* const data, const packets::packet_length length ) {
	std::uint32_t bytes_sent = 0;
	do {
		int sent = send( 
			to, 
			reinterpret_cast< char* >( data ) + bytes_sent, 
			length - bytes_sent, 
			0 
		);

		if ( sent <= 0 )
			return false;

		bytes_sent += sent;
	} while ( bytes_sent < length );

	return true;
}

void async_tcp_server::disconnect_marked_clients( ) {
	std::lock_guard guard1( client_mtx_ );
	std::lock_guard guard2( process_mtx_ );
	std::lock_guard guard3( disconnect_mtx_ );

	for ( auto client : clients_to_disconnect_ ) {
		auto it = std::find_if( connected_clients_.begin( ), connected_clients_.end( ), [ &client ]( const SOCKET& s ) {
			return s == client;
		} );

		if ( it == connected_clients_.end( ) )
			return;

		if ( on_disconnect_callback_ )
			on_disconnect_callback_( this, client );

		shutdown( client, SD_BOTH );
		closesocket( client );

		process_buffers_.erase( client );
		connected_clients_.erase( it );
	}

	clients_to_disconnect_.clear( );
}

void async_tcp_server::accept_clients( ) {
	while ( running_ ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		auto client = accept( server_socket_, nullptr, nullptr );

		if ( client == INVALID_SOCKET )
			continue;

		// Attempt to handshake with the client,
		// disconnect from it upon failure.
		if ( !perform_handshake( client ) ) {
			shutdown( client, SD_BOTH );
			closesocket( client );
			continue;
		}

		std::lock_guard guard( client_mtx_ );
		connected_clients_.push_back( client );

		if ( !on_connect_callback )
			continue;

		on_connect_callback( this, client );
	}
}

// TODO: finish processing all packets before we exit thread (cba rn, but its ez. look @ client)
void async_tcp_server::process_data( ) {
	while ( running_ ) { // The server will only process data for as long as it's running
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		// Disconnect all marked clients before we process any data
		disconnect_marked_clients( );

		std::lock_guard guard( process_mtx_ );

		for ( auto& client : connected_clients_ ) {
			auto& process_buffer = process_buffers_[ client ];

			if ( process_buffer.size( ) < sizeof( packets::header ) )
				continue;

			auto header = reinterpret_cast< packets::header* >( process_buffer.data( ) );

			bool is_disconnect_packet = header->id == packets::ids::id_disconnect && header->flags & packets::flags::fl_disconnect;

			// Disconnect if we receive some malformed packet or
			// when the client wants to disconnect
			if ( header->magic != PACKET_MAGIC || is_disconnect_packet ) {
				disconnect_client( client );
				continue;
			}

			// We have received a full packet
			if ( process_buffer.size( ) < header->length )
				continue;

			std::uint32_t data_length = header->length - sizeof( packets::header );

			// Check if we have a callback available
			if ( callbacks_.find( header->id ) != callbacks_.end( ) ) {
				auto data_start = process_buffer.data( ) + sizeof( packets::header );

				// Assign the data to our serializer
				serializer.assign_buffer( data_start, data_length );

				callbacks_[ header->id ]( this, client, serializer );
			}

			// Erase the packet from our buffer
			process_buffer.erase( process_buffer.begin( ), process_buffer.begin( ) + data_length + sizeof( packets::header ) );
		}
	}
}

void async_tcp_server::receive_data( ) {
	std::vector< std::uint8_t > buffer( buffer_size_ );

	while ( running_ ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		std::lock_guard guard( client_mtx_ );
		for ( auto& client : connected_clients_ ) {
			// Check if we received any data from our client
			// so we don't block the thread with recv
			unsigned long available_to_read = 0;
			ioctlsocket( client, FIONREAD, &available_to_read );

			if ( !available_to_read )
				continue;

			int bytes_received = recv( client, reinterpret_cast< char* >( buffer.data( ) ), buffer_size_, 0 );

			switch ( bytes_received ) {
				case -1: 
					// Disconnect the client on error
					// We don't disconnect him on code 0 as that
					// implies the client closed the connection by
					// himself, which means it should've sent a 
					// disconnect packet.
					disconnect_client( client );
					break;
				case 0:
					break;
				default: // Received bytes, process them
					std::lock_guard guard( process_mtx_ );

					auto& process_buffer = process_buffers_[ client ];
					process_buffer.insert( process_buffer.end( ), buffer.begin( ), buffer.begin( ) + bytes_received );
			}
		}
	}
}