#include "bin_serializer.h"

using namespace fi::packets::detail;

void binary_serializer::serialize( const std::string& item ) {
	write_to_buffer< std::uint32_t >( item.length( ) );
	serialized_buffer_.insert( serialized_buffer_.end( ), item.begin( ), item.end( ) );
}

void binary_serializer::serialize( const std::vector< std::string >& item ) {
	write_to_buffer< std::uint32_t >( item.size( ) );

	for ( auto& s : item )
		serialize( s );
}

void binary_serializer::deserialize( std::string& out_item ) {
	auto length = read_from_buffer< std::uint32_t >( );
	
	out_item.resize( length );
	memcpy( out_item.data( ), serialized_buffer_.data( ) + deserialized_bytes_, length );
	
	deserialized_bytes_ += length;
}

void binary_serializer::deserialize( std::vector< std::string >& out_item ) {
	auto num_strings = read_from_buffer< std::uint32_t >( );

	out_item.resize( num_strings );
	for ( std::uint32_t i = 0; i < num_strings; i++ ) {
		std::string deserialized = "";
		deserialize( deserialized );

		out_item[ i ] = std::move( deserialized );
	}
}

std::uint8_t* binary_serializer::get_serialized_data( ) {
	return serialized_buffer_.data( );
}

std::uint32_t binary_serializer::get_serialized_data_length( ) {
	return serialized_buffer_.size( );
}

void binary_serializer::reset( ) {
	deserialized_bytes_ = 0;
	serialized_buffer_.clear( );
}

void binary_serializer::assign_buffer( std::uint8_t* const data, std::uint32_t length ) {
	reset( );
	serialized_buffer_.insert( serialized_buffer_.begin( ), data, data + length );
}