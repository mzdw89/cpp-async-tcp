#include "async_client/async_client.h"
#include <iostream>

int main( ) {
	try {
		fi::async_tcp_client client = { };

		client.register_disconnect_callback( [ ]( fi::async_tcp_client* const cl ) {
			printf( "Disconnected from server.\n" );
		} );

		client.register_callback( fi::packets::ids::id_example, [ ]( fi::async_tcp_client* const cl, fi::packets::detail::binary_serializer& s ) {
			// Read our packet
			fi::packets::example_packet example( s );

			// Now we can access our data
			for ( std::size_t i = 0; i < example.some_string_array.size( ); i++ )
				printf( "[ %i ] %s\n", i, example.some_string_array[ i ].data( ) );

			// Disconnect from our server, as we're done communicating.
			cl->disconnect( );
		} );

		if ( client.connect( "localhost", "1337" ) ) {
			printf( "Connected to server!\n" );

			// Craft a packet once we're connected
			fi::packets::example_packet example = { };

			example.some_short = 128;
			example.some_array = { 1, 2, 3, 4, 5 };
			example.some_string_array = { "Hello", "from", "client!" };

			// Send the packet
			client.send_packet( &example );

			// Wait for the server to answer
			while ( client.is_connected( ) ) {
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			}
		} else
			printf( "Handshake has failed.\n" );

		std::cin.get( );
		return 0;
	} catch ( const std::exception& e ) {
		printf( "%s\n", e.what( ) );
		
		std::cin.get( );
		return 1;
	}
}
