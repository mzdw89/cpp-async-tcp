#include "async_server/async_server.h"

int main( ) {
	try {
		fi::async_tcp_server server = { };
		bool server_should_run = true;

		// Setup all our callbacks before starting the server
		server.register_connect_callback( [ ]( fi::async_tcp_server* const sv, SOCKET who ) {
			printf( "Client with socket ID %i has connected.\n", who );
		} );

		server.register_disconnect_callback( [ &server_should_run ]( fi::async_tcp_server* const sv, SOCKET who ) {
			printf( "Client with socket ID %i has disconnected.\n", who );

			// Stop the server (do NOT call sv->stop( ) here, as that will result 
			// in infinite recursion. ALWAYS stop the server in the same thread you
			// started it in!)
			server_should_run = false;
		} );

		server.register_stop_callback( [ ]( fi::async_tcp_server* const sv ) {
			printf( "Server has been stopped.\n" );
		} );

		server.register_callback( fi::packets::ids::id_example, [ ]( fi::async_tcp_server* const sv, SOCKET from, fi::packets::detail::binary_serializer& s ) {
			// Read our packet
			fi::packets::example_packet example( s );

			// Now we can access our data
			for ( std::size_t i = 0; i < example.some_string_array.size( ); i++ )
				printf( "[ %i ] %s\n", i, example.some_string_array[ i ].data( ) );

			// Answer the client
			example.some_string_array = { "Hello", "from", "server!" };

			sv->send_packet( from, &example );
		} );

		// Attempt to start the server
		server.start( "1337" );

		printf( "Server running on port 1337.\n" );

		// Wait for our server to stop running
		while ( server_should_run ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}

		// Officialy stop the server
		server.stop( );

		return 0;
	} catch ( const std::exception& e ) {
		printf( "%s\n", e.what( ) );
		return 1;
	}
}