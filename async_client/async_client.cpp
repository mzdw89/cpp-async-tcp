#include "async_client.h"

using namespace fi;

async_tcp_client::async_tcp_client( ) {
#ifdef _WIN32
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa_data_ ) != 0 )
		throw std::exception( "async_tcp_client::connect: WSAStartup failed" );
#endif // _WIN32
}

async_tcp_client::~async_tcp_client( ) {
	disconnect( );

	if ( processing_thread_.joinable( ) )
		processing_thread_.join( );
	
	if ( receiving_thread_.joinable( ) )
		receiving_thread_.join( );

#ifdef _WIN32
	WSACleanup( );
#endif // _WIN32
}

bool async_tcp_client::connect( std::string_view ip, std::string_view port ) {
	if ( connected_ )
		throw std::exception( "async_tcp_client::connect: attempted to connect while a connection was open" );

	addrinfo hints = { }, * result = nullptr;

	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	if ( getaddrinfo( ip.data( ), port.data( ), &hints, &result ) != 0 )
		throw std::exception( "async_tcp_client::connect: getaddrinfo error" );

	socket_ = ::socket( result->ai_family, result->ai_socktype, result->ai_protocol );

	if ( socket_ == INVALID_SOCKET ) {
		freeaddrinfo( result );
		throw std::exception( "async_tcp_client::connect: failed to create socket" );
	}

	if ( ::connect( socket_, result->ai_addr, int( result->ai_addrlen ) ) == SOCKET_ERROR ) {
		freeaddrinfo( result );
		throw std::exception( "async_tcp_client::connect: error connecting" );
	}

	freeaddrinfo( result );

	if ( !perform_handshake( ) ) {
		disconnect_internal( disconnect_reasons::reason_handshake_fail );
		return false;
	}

	connected_ = true;

	processing_thread_ = std::thread( &async_tcp_client::process_data, this );
	receiving_thread_ = std::thread( &async_tcp_client::receive_data, this );

	return true;
}

void async_tcp_client::disconnect( ) {
	if ( connected_ )
		disconnect_internal( disconnect_reasons::reason_stop );
}

bool async_tcp_client::is_connected( ) {
	return connected_;
}

void async_tcp_client::send_packet( packets::base_packet* const packet ) {
	if ( !packet )
		throw std::exception( "async_tcp_client::send_packet: packet was nullptr" );

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
	if ( !send_packet_internal( packet_data.data( ), packet_data.size( ) ) )
		disconnect_internal( disconnect_reasons::reason_error );
}

void async_tcp_client::register_callback( packets::packet_id packet_id, packet_callback_client_fn callback_fn ) {
	if ( !callback_fn )
		throw std::exception( "async_tcp_client::register_callback: no callback given" );

	callbacks_[ packet_id ] = callback_fn;
}

void async_tcp_client::remove_callback( packets::packet_id packet_id ) {
	callbacks_.erase( packet_id );
}

void async_tcp_client::set_callback_map( const std::unordered_map< packets::packet_id, packet_callback_client_fn >& callback_map ) {
	callbacks_ = callback_map;
}

void async_tcp_client::register_disconnect_callback( std::function< void( async_tcp_client* const ) > callback_fn ) {
	on_disconnect_callback_ = callback_fn;
}

packets::header async_tcp_client::construct_packet_header( packets::packet_length length, packets::packet_id id, packets::packet_flags flags ) {
	packets::header packet_header = { };

	packet_header.flags = flags;
	packet_header.id = id;
	packet_header.length = sizeof( packets::header ) + length;
	packet_header.magic = PACKET_MAGIC;

	return packet_header;
}

bool async_tcp_client::perform_handshake( ) {
	packets::header packet_header = construct_packet_header( 0, packets::ids::id_handshake, packets::flags::fl_handshake_cl );

	auto buffer = reinterpret_cast< char* >( &packet_header );

	// Send our header with no body and the handshake_cl flag
	if ( !send_packet_internal( &packet_header, sizeof( packets::header ) ) )
		return false;

	// Receive a response back. Should be the header with handshake_sv flag
	int bytes_received = 0;
	do {
		int received = recv( socket_, buffer + bytes_received, sizeof( packets::header ) - bytes_received, 0 );

		if ( received <= 0 )
			return false;

		bytes_received += received;
	} while ( bytes_received < sizeof( packets::header ) );

	// Check the header information for the information we are expecting
	if ( packet_header.flags != packets::flags::fl_handshake_sv )
		return false;

	if ( packet_header.id != packets::ids::id_handshake )
		return false;

	if ( packet_header.length != sizeof( packets::header ) )
		return false;

	if ( packet_header.magic != PACKET_MAGIC )
		return false;

	return true;
}

bool async_tcp_client::send_packet_internal( void* const data, const packets::packet_length length ) {
	std::uint32_t bytes_sent = 0;
	do {
		int sent = send(
			socket_,
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

void async_tcp_client::disconnect_internal( const disconnect_reasons reason ) {
	// This may be called from multiple threads
	std::lock_guard guard( disconnect_mtx_ );

	connected_ = false;

	switch ( reason ) {
		case disconnect_reasons::reason_handshake_fail:
			if ( socket_ ) {
				shutdown( socket_, SD_BOTH );
				closesocket( socket_ );
				socket_ = 0;
			}
			break;
		case disconnect_reasons::reason_stop: // Send a disconnect packet as the client has requested a disconnect
		{
			packets::header packet_header = construct_packet_header( 0, packets::ids::id_disconnect, packets::flags::fl_disconnect );
			send_packet_internal( &packet_header, sizeof( packets::header ) );
		}
		case disconnect_reasons::reason_error:
		case disconnect_reasons::reason_server_stop:
			if ( socket_ ) {
				shutdown( socket_, SD_BOTH );
				closesocket( socket_ );
				socket_ = 0;

				if ( on_disconnect_callback_ )
					on_disconnect_callback_( this );
			}
	}
}

void async_tcp_client::process_data( ) {
	while ( true ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		std::lock_guard guard( process_mtx_ );

		if ( !connected_ ) {
			// Nothing left to process, exit
			if ( process_buffer_.size( ) < sizeof( packets::header ) )
				break;

			auto header = reinterpret_cast< packets::header* >( process_buffer_.data( ) );

			// Data partially received, exit
			if ( process_buffer_.size( ) < header->length )
				break;

			// Keep processing data as we have packets left
		}

		if ( process_buffer_.size( ) < sizeof( packets::header ) )
			continue;

		auto header = reinterpret_cast< packets::header* >( process_buffer_.data( ) );

		// Disconnect if we receive some malformed packet
		if ( header->magic != PACKET_MAGIC ) {
			connected_ = false;
			break;
		}

		// We have received a full packet
		if ( process_buffer_.size( ) < header->length )
			continue;
	
		std::uint32_t data_length = header->length - sizeof( packets::header );
		
		// Check if we have a callback available
		if ( callbacks_.find( header->id ) != callbacks_.end( ) ) {
			auto data_start = process_buffer_.data( ) + sizeof( packets::header );

			// Assign the data to our serializer
			serializer.assign_buffer( data_start, data_length );

			callbacks_[ header->id ]( this, serializer );
		}

		// Erase the packet from our buffer
		process_buffer_.erase( process_buffer_.begin( ), process_buffer_.begin( ) + data_length + sizeof( packets::header ) );
	}

	process_buffer_.clear( );
}

void async_tcp_client::receive_data( ) {
	std::vector< std::uint8_t > buffer( buffer_size_ );

	while ( connected_ ) {
		int bytes_received = recv( socket_, reinterpret_cast< char* >( buffer.data( ) ), buffer_size_, NULL );

		switch ( bytes_received ) {
			case -1: // An error occurred
				disconnect_internal( disconnect_reasons::reason_error );
				break;
			case 0: // Server disconnected us 
				disconnect_internal( disconnect_reasons::reason_server_stop );
				break;
			default:
				std::lock_guard guard( process_mtx_ );

				process_buffer_.insert( process_buffer_.end( ), buffer.begin( ), buffer.begin( ) + bytes_received );
		}

		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );	
	}
}