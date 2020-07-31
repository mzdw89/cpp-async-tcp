# cpp-async-tcp
Asynchronous server/client implementation in C++.

## Getting started
Download/clone the repository and include the files in your project.
Please take a look at the example files `client_main.cpp` and `server_main.cpp` before attempting to use this library to familiarize yourself with the structure and logic.

## Function descriptions
### Client
```c++
bool async_tcp_client::connect( std::string_view ip, std::string_view port );
```
`connect` is used to establish a connection with the server.
Possible return values:
- true: The connection was established successfully.
- false: The connection was established, but the handshake failed.
- exception: The connection could not be established or a connection is already open.
```c++
void async_tcp_client::disconnect( );
```
`disconnect` is used to close a connection with the server. It will shut the connection down properly by first notifying the server about the disconnect, and then closing the socket.
```c++
bool async_tcp_client::is_connected( );
```
`is_connected` will return whether or not the client is currently connected to a server.
```c++
void async_tcp_client::send_packet( packets::base_packet* const packet );
```
`send_packet` is used to send a packet to the server. Upon failure, the connection will be closed. 
An exception will be thrown if the pointer is invalid and you will be disconnected from the server.
```c++
void async_tcp_client::register_callback( packets::packet_id packet_id, packet_callback_client_fn callback_fn );
```
`register_callback` is used to register a callback which will be called once a packet with the corresponding ID is received. If no callback is registered for a given ID, the packet is discarded.
```c++
void async_tcp_client::remove_callback( packets::packet_id packet_id );
```
`remove_callback` will remove a callback for a given packet ID.
```c++
void async_tcp_client::set_callback_map( const std::unordered_map< packets::packet_id, packet_callback_client_fn >&
callback_map );
```
`set_callback_map` is useful for when you have a lot of callbacks to register. Please note that it will override all existing callbacks with the map provided.
```c++
void async_tcp_client::register_disconnect_callback( std::function< void( async_tcp_client* const ) > callback_fn );
```
`register_disconnect_callback` will register a callback which will be called upon the client being disconnected from the server, be it due to an internal failure or due to `disconnect` being called.

### Server
```c++
void async_tcp_server::start( std::string_view port );
```
`start` will start the server on the given port. Upon error, an exception will be thrown.
```c++
void async_tcp_server::stop( );
```
`stop` will disconnect all clients and stop the server.
```c++
void async_tcp_server::disconnect_client( SOCKET who );
```
`disconnect_client` is used to disconnect a client from the server. Upon doing so, if given, the disconnect callback will be called.
```c++
bool async_tcp_server::is_running( );
```
`is_running` will return whether or not the server is currently running.
```c++
void async_tcp_server::send_packet( SOCKET to, packets::base_packet* packet );
```
`send_packet` will send a packet to the given client. Upon failure, the client will be disconnected from the server.
```c++
void async_tcp_server::register_callback( packets::packet_id packet_id, packet_callback_server_fn callback_fn );
```
Same as client.
```c++
void async_tcp_server::remove_callback( packets::packet_id packet_id );
```
Same as client.
```c++
void async_tcp_server::set_callback_map( const std::unordered_map< packets::packet_id, packet_callback_server_fn >& callback_map );
```
Same as client.
```c++
void async_tcp_server::register_stop_callback( std::function< void( async_tcp_server* const ) > callback_fn );
```
`register_stop_callback` will register a callback which will be called once the server is stopped using `stop` or the deconstructor.
```c++
void async_tcp_server::register_connect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn );
```
`register_connect_callback` is used to register a callback which will be called notifying the user that a client successfully connected. The callback will not be called if the handshake with the client fails. 
```c++
void async_tcp_server::register_disconnect_callback( std::function< void( async_tcp_server* const, const SOCKET ) > callback_fn );
```
Same as client.

## Packets
Here's what you need to do to implement your own packets:
- In `packet_base.h`:
    - Add a packet ID in the `ids` enum.
- In `packets.h`:
    - Create a new class based off of `base_packet`.
    - In your class, override and implement the methods `serialize`, `deserialize` and `get_id`.
    
    This is an example implementation of a class: 
    ```c++
    // Templated because that way we don't have to create 
    // a new class every time we want to send text
    template < packet_id id >
    class text_packet : public base_packet {
        // Make sure to (de-)serialize in the same order!
        virtual void serialize( detail::binary_serializer& s ) {
            s.serialize( text );
        }
    
        virtual void deserialize( detail::binary_serializer& s ) {
            s.deserialize( text );
        }
    
        virtual packet_id get_id( ) {
            return id;
        }
    
        std::string text = "";
    };
    ```
    
    Currently allowed datatypes to serialize are as follows:
    - Any arithmetic datatype
        - std::int8_t / std::uint8_t
        - std::int16_t / std::uint16_t
        - std::int32_t / std::uint32_t
        - std::int64_t / std::uint64_t
        - float / double
    - Arrays
        - std::string
        - std::vector< arithmetic_datatype >
        - std::vector< std::string >
        
    If you want to implement serializiation for more datatypes, take a look at `binary_serializer`.
    
### License
This project is licensed under the MIT license.
