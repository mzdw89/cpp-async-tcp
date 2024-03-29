#include "async_server/async_server.h"

#define PROCESS_PACKET_FN(ID, name) void name( fi::async_tcp_server* const sv, const SOCKET from, const fi::packets::packet_id id, fi::packets::detail::binary_serializer& s)

PROCESS_PACKET_FN( fi::packets::id_example, on_example_packet ) {
	// Read our packet
	fi::packets::example_packet example( s );

	// Now we can access our data
	for ( std::size_t i = 0; i < example.some_string_array.size( ); i++ )
		printf( "[ %i ] %s\n", i, example.some_string_array[ i ].data( ) );

	// Answer the client
	example.some_string_array = { "Hello", "from", "server!" };

	sv->send_packet( from, &example );
}

int main( ) {
	try {
		fi::async_tcp_server server = { };

		// Setup all our callbacks before starting the server
		server.register_connect_callback( [ ]( fi::async_tcp_server* const sv, SOCKET who ) {
			printf( "Client with socket ID %i has connected.\n", who );
		} );

		server.register_disconnect_callback( [ ]( fi::async_tcp_server* const sv, SOCKET who ) {
			printf( "Client with socket ID %i has disconnected.\n", who );
			
			// Stop the server as we're done communicating
			sv->stop( );
		} );

		server.register_stop_callback( [ ]( fi::async_tcp_server* const sv ) {
			printf( "Server has been stopped.\n" );
		} );

		server.register_callback( [ ]( fi::async_tcp_server* const sv, SOCKET from, const fi::packets::packet_id id, fi::packets::detail::binary_serializer& s ) {
			// You can use a switch case, an unordered map, an array.. whichever suits you best
			switch ( id ) {
				case fi::packets::id_example:
					on_example_packet( sv, from, id, s );
					break;
				default:
					printf( "Unknown packet ID %i received\n", id );
			}
		} );

		// Attempt to start the server
		server.start( "1337" );

		printf( "Server running on port 1337.\n" );

		// Wait for our server to stop running
		while ( server.is_running( ) ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}

		return 0;
	} catch ( const fi::async_tcp_server::exception& e ) {
		printf( "%s\n", e.what( ) );
		return 1;
	}
}